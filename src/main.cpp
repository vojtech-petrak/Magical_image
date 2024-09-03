#include "storage.h"
#include "epd.h"
#include "server_access.h"
#include <Arduino.h>
#include <SD.h>
#include <EEPROM.h>
#include <tuple>

using namespace std;

void tryReportErrors(const FullResult *const errors, uint8_t *const error_count) {
    if (!*error_count) return;
    const ReportResult result = reportErrors(errors, *error_count);
    if (result == ReportResult::Ok) *error_count = 0;
    else {
        uint8_t old_error_count = EEPROM.read(ERROR_COUNT_ADDRESS);
        if (old_error_count == 255) old_error_count = 0;
        for (uint8_t i = old_error_count; i < *error_count; i += 1) EEPROM.write(i, errors[i].value);
        writeError(error_count, Type::DayGeneric, (Result)result);
    }
}

// days_until_recent_check
uint8_t checkRecent(FullResult *const errors, uint8_t *const error_count, uint8_t days_until_recent_check) {
    if (days_until_recent_check != 0) return days_until_recent_check;

    const auto [result, days_until_current_check_new] = saveRecent();
    if (days_until_current_check_new != 0 && days_until_current_check_new <= DAYS_UNTIL_RECENT_CHECK_WARNING)
        days_until_recent_check = days_until_current_check_new;
    else if (result != SaveResult::LimitExceded) writeError(errors, error_count, Type::DayRecent, Result::LimitExceded);
    if (result != SaveResult::Ok) writeError(errors, error_count, Type::DayRecent, (Result)result);

    return days_until_recent_check;
}
// min_file_count, rand_files_shifted
tuple<uint8_t, bool> checkRand(FullResult *const errors, uint8_t *const error_count, uint8_t min_file_count, uint8_t images_read) {
    auto [count_success, rand_file_count] = randFileCount();
    if (!count_success) writeError(errors, error_count, Type::DayGeneric, Result::FilesMissing);
    if (rand_file_count - images_read >= min_file_count) return tuple(min_file_count, false);

    bool rand_files_shifted = false;
    const auto [result, images, min_file_count_new] = saveRand(rand_file_count);
    if (min_file_count_new != 0 && min_file_count_new <= MIN_FILE_COUNT_WARNING) min_file_count = min_file_count_new;
    else if (result != SaveResult::LimitExceded) writeError(errors, error_count, Type::DayRand, Result::LimitExceded);
    if (result == SaveResult::Ok) {
        RandFilesResult result = shiftFiles(rand_file_count + images, images_read);
        if (result != RandFilesResult::Ok) writeError(errors, error_count, Type::DayRand, (Result)result);
        rand_files_shifted = true;
    } else writeError(errors, error_count, Type::DayRand, (Result)result);
    return tuple(min_file_count, rand_files_shifted);
}
// days_until_battery_check, terminate
tuple<uint8_t, bool> checkBattery(FullResult *const errors, uint8_t *const error_count, uint8_t days_until_battery_check) {
    if (days_until_battery_check != 0) return tuple(days_until_battery_check, false);

    uint8_t terminate = false;
    const uint16_t charge = analogRead(A0);
    const ReportResult battery_result = reportBattery(charge);
    if (battery_result == ReportResult::Ok) {
        days_until_battery_check = DAYS_UNTIL_BATTERY_CHECK;
        if (charge < BATTERY_CHARGE_WARNING) {
            clearEpd();
            terminate = true;
        }
    } else writeError(errors, error_count, Type::DayGeneric, (Result)battery_result);
    return tuple(days_until_battery_check, terminate);
}

uint8_t readHour(uint8_t *const error_count) {
    uint32_t saved_hour;
    if (!ESP.rtcUserMemoryRead(0, &saved_hour, 1)) { writeError(error_count, Type::Generic, Result::RtcReadFailed); return 255; }
    const uint8_t hour = saved_hour & 0xff;
    if (hour != saved_hour >> 8) { writeError(error_count, Type::Generic, Result::TimeLost); return 255; }
    if (hour > 24) {
        if (hour != 255) writeError(error_count, Type::Generic, Result::LimitExceded);
        return 255;
    }
    return hour;
}

void sleep(uint8_t error_count, uint8_t hour, const bool terminate = analogRead(A0) < BATTERY_CHARGE_ERROR) {
    if (hour < 24) {
        hour += 3;
        if (hour >= 24) hour -= 24;
    }
    if (terminate) hour = 255;
    uint32_t saved_hour = ((uint16_t)hour << 8) | hour;
    if (!ESP.rtcUserMemoryWrite(0, &saved_hour, 1)) writeError(&error_count, Type::Generic, Result::RtcWriteFailed);

    if (error_count != EEPROM.read(ERROR_COUNT_ADDRESS)) {
        EEPROM.write(ERROR_COUNT_ADDRESS, error_count);
        EEPROM.commit();
    }
    //Serial.println("sleep");
    //for (uint8_t i = 0; i < 18; i += 1) Serial.println(EEPROM.read(i)); // TODO: remove
    EEPROM.end();

    if (terminate) ESP.deepSleep(0);
    ESP.deepSleep(10800e6 - micros()); // 3 hours
}
void tryReprotSd(uint8_t error_count) {
    if (error_count == 255) sleep(error_count, 255); // reported

    WiFi.mode(WIFI_STA);
    WiFi.begin(SSID, PSW);
    if (!EEPROM.read(EPD_CLEARED_ADDRESS)) {
        writeError(&error_count, Type::Generic, Result::SdNotConnected);
        clearEpd();
    }

    if (WiFi.waitForConnectResult() != WL_CONNECTED) {
        disconnectWifi();
        sleep(error_count, 255);
    }

    FullResult errors[ERROR_BUFFER_SIZE];

    for (uint8_t i = 0; i < error_count; i += 1) errors[i] = (FullResult)EEPROM.read(i);

    tryReportErrors(errors, &error_count);
    disconnectWifi();

    if (!error_count) error_count = 255;
    sleep(error_count, 255);
}

void setup() {
    // TODO: remove
    //Serial.begin(9600);
    //while (!Serial);
    //Serial.println();
    //Serial.println("start");

    pinMode(Epd::CS_PIN, OUTPUT);
    digitalWrite(Epd::CS_PIN, HIGH);

    EEPROM.begin(18);
    uint8_t error_count = EEPROM.read(ERROR_COUNT_ADDRESS);
    if (error_count == 255) error_count = 0;

    uint8_t hour = readHour(&error_count);
    //Serial.println(hour);
    if (error_count == 255) hour = 255;
    if ((hour > 2 && hour < 12) || (hour > 14 && hour < 24)) sleep(error_count, hour);

    // read //

    if (hour == 255 || (hour >= 12 && hour <= 14)) {
        WiFi.mode(WIFI_STA);
        WiFi.begin(SSID, PSW);
    }

    if (!SD.begin(SD_CS)) {
        disconnectWifi();
        tryReprotSd(error_count);
    }

    uint8_t next_image = 0;
    uint8_t images_read = 0;
    uint8_t days_until_recent_check = 0;
    uint8_t days_until_battery_check = 0;

    uint8_t failed_wifi_connections = 0;
    uint8_t min_file_count = 30;

    yield();
    File state = SD.open(STATE_FILE);
    if (!state) writeError(&error_count, Type::Generic, Result::ReadOpenFailed);
    else if (state.available() != 6) { state.close(); writeError(&error_count, Type::Generic, Result::WrongLength); }
    else {
        next_image = state.read();
        images_read = state.read();

        days_until_recent_check = state.read();
        if (days_until_recent_check > DAYS_UNTIL_RECENT_CHECK_WARNING) {
            writeError(&error_count, Type::Generic, Result::LimitExceded);
            days_until_recent_check = 0;
        }
        days_until_battery_check = state.read();
        if (days_until_battery_check > DAYS_UNTIL_BATTERY_CHECK) {
            writeError(&error_count, Type::Generic, Result::LimitExceded);
            days_until_battery_check = 0;
        }
        failed_wifi_connections = state.read();
        if (failed_wifi_connections > MAX_FAILED_WIFI_CONNECTIONS) {
            writeError(&error_count, Type::Generic, Result::LimitExceded);
            failed_wifi_connections = MAX_FAILED_WIFI_CONNECTIONS;
        }
        min_file_count = state.read();
        if (min_file_count == 0 || min_file_count > MIN_FILE_COUNT_WARNING) {
            writeError(&error_count, Type::Generic, Result::LimitExceded);
            min_file_count = DEFAULT_MIN_FILE_COUNT;
        }
        state.close();
    }

    if (failed_wifi_connections > MAX_FAILED_WIFI_CONNECTIONS) {
        WiFi.mode(WIFI_STA);
        WiFi.begin(SSID, PSW);
    }

    // connect //

    bool terminate = false;

    if (hour == 255) {
        if (WiFi.waitForConnectResult() == WL_CONNECTED) {
            if ((hour = getNtpHour()) < 24) goto connected;
            else writeError(&error_count, Type::Generic, Result::NtpUpdateFailed);
        }
        disconnectWifi();
        clearEpd();
        SD.end();
        sleep(error_count, 255);
    } else if ((hour >= 12 && hour <= 14) || failed_wifi_connections == MAX_FAILED_WIFI_CONNECTIONS) {
        if (WiFi.waitForConnectResult() == WL_CONNECTED) {
            const uint8_t new_hour = getNtpHour();
            if (new_hour < 24) hour = new_hour;
            else writeError(&error_count, Type::DayGeneric, Result::NtpUpdateFailed);
            goto connected;
        }
        disconnectWifi();
        if (failed_wifi_connections < MAX_FAILED_WIFI_CONNECTIONS) {
            failed_wifi_connections += 1;
            if (failed_wifi_connections == MAX_FAILED_WIFI_CONNECTIONS) writeError(&error_count, Type::DayGeneric, Result::TimeLost);
        }
    }

    // compute //

    if (false) {
        connected: 

        FullResult errors[ERROR_BUFFER_SIZE];
        for (uint8_t i = 0; i < error_count; i += 1) errors[i] = (FullResult)EEPROM.read(i);

        days_until_recent_check = checkRecent(errors, &error_count, days_until_recent_check);
        bool rand_files_shifted;
        tie(min_file_count, rand_files_shifted) = checkRand(errors, &error_count, min_file_count, images_read);
        if (rand_files_shifted) {
            images_read = 0;
            next_image = 0;
        }

        tie(days_until_battery_check, terminate) = checkBattery(errors, &error_count, days_until_battery_check);

        tryReportErrors(errors, &error_count);
        disconnectWifi();
    }
    
    if (hour <= 2) {
        const auto [rand_file_count, new_next_image, rollover] = night(next_image, &error_count);
        next_image = new_next_image;
        images_read = rollover ? rand_file_count : std::max(images_read, next_image);
        if (days_until_recent_check != 0) days_until_recent_check -= 1;

        if (days_until_battery_check != 0) days_until_battery_check -= 1;
        const uint16_t charge = analogRead(A0);
        if (charge < BATTERY_CHARGE_ERROR) terminate = true;
        else if (charge < BATTERY_CHARGE_WARNING) days_until_battery_check = 0;
    }

    // write back //

    yield();
    if ((state = SD.open(STATE_FILE, FILE_WRITE))) {
        if (!state.truncate(0)) writeError(&error_count, Type::Generic, Result::ClearFailed);

        if (!state.write(next_image)) goto write_end;
        if (!state.write(images_read)) goto write_end;
        if (!state.write(days_until_recent_check)) goto write_end;
        if (!state.write(days_until_battery_check)) goto write_end;
        if (!state.write(failed_wifi_connections)) goto write_end;
        if (!state.write(min_file_count)) goto write_end;

        if (false) { write_end: writeError(&error_count, Type::Generic, Result::WriteFailed); }

        state.close();
    } else writeError(&error_count, Type::Generic, Result::WriteOpenFailed);

    SD.end();
    sleep(error_count, hour, terminate);
}

void loop() {}
