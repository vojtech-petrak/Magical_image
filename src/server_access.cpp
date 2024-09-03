// wifi: https://arduino-esp8266.readthedocs.io/en/latest/esp8266wifi/readme.html
// http: https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266HTTPClient/examples/StreamHttpClient/StreamHttpClient.ino

#include "server_access.h"
#include "epd.h"
#include "storage.h"
#include <SPI.h>
#include <SD.h>
#include <tuple>
#include <NTPClient.h>
#include <WiFiUdp.h>
//#include <wl_definitions.h>
//#include "ESP8266WiFiGeneric.h"

using namespace std;

void disconnectWifi() {
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
}

ReportResult reportBattery(const uint8_t charge) {
    WiFiClient wifi;
    HTTPClient http;

    if (!http.begin(wifi, REPORT)) return ReportResult::HttpBeginFailed;
    const uint8_t message[3] = { 255, (uint8_t)(charge & 0x00ff), (uint8_t)(charge >> 8) };
    const ReportResult result = http.POST(message, sizeof(message)) == 200 ? ReportResult::Ok : ReportResult::HttpRequestFailed;

    http.end();
    return result;
}

ReportResult reportErrors(const FullResult *const errors, const uint8_t error_count) {
    WiFiClient wifi;
    HTTPClient http;

    if (!http.begin(wifi, REPORT)) return ReportResult::HttpBeginFailed;
    const ReportResult result = http.POST((uint8_t *)errors, error_count) == 200 ? ReportResult::Ok : ReportResult::HttpRequestFailed;

    http.end();
    return result;
}

uint8_t getNtpHour() {
    WiFiUDP udp;
    NTPClient client(udp);
    client.begin();
    if (!client.update()) return 255;
    return client.getHours();
}

bool wait(WiFiClient *const stream) {
    uint8_t tries = 0;
    while (!stream->available()) {
        if (++tries == 32) return false;
        delay(10);
    }
    return true;
}

DownloadResult download(WiFiClient *const stream, const char *const file_name) {
    yield();
    DownloadResult result;
    File file = SD.open(file_name, FILE_WRITE);
    if (!file) return DownloadResult::CreateFailed;
    if (!file.truncate(0)) { result = DownloadResult::ClearFailed; goto end; }
    {
        if (!wait(stream)) { result = DownloadResult::TooShort; goto end; }
        uint16_t height = stream->read();
        if (!wait(stream)) { result = DownloadResult::TooShort; goto end; }
        height |= stream->read() << 8;
        if (height > Epd::HEIGHT) { result = DownloadResult::TooLarge; goto end; }

        if (!wait(stream)) { result = DownloadResult::TooShort; goto end; }
        uint16_t width = stream->read();
        if (!wait(stream)) { result = DownloadResult::TooShort; goto end; }
        width |= stream->read() << 8;
        if (width > Epd::WIDTH) { result = DownloadResult::TooLarge; goto end; }
        if (!file.write((uint8_t)(width & 0xff)) || !file.write((uint8_t)(width >> 8))) { result = DownloadResult::WriteFailed; goto end; }

        const uint32_t byte_count = (uint32_t)height * (uint32_t)width;
        uint8_t failed_read_count = 0;
        for (uint32_t written = 0; written < byte_count;) {
            uint8_t buf[256];
            if (!stream->connected()) { result = DownloadResult::StreamNotConnected; goto end; }
            if (!wait(stream)) { result = DownloadResult::TooShort; goto end; }
            int read_len = stream->read(buf, std::min((size_t)(byte_count - written), sizeof(buf)));
            if (read_len <= 0) {
                if (++failed_read_count > 32) { result = DownloadResult::StreamReadFailed; goto end; }
                delay(10);
                continue;
            }
            if (!file.write(buf, read_len)) { result = DownloadResult::WriteFailed; goto end; }
            written += read_len;
        }
    }
    result = DownloadResult::Ok;

    end:
    file.close();
    return result;
}

// result, days_until_current_check
tuple<SaveResult, uint8_t> saveRecent() {
    SaveResult result;
    WiFiClient wifi;
    HTTPClient http;
    WiFiClient *stream;
    uint8_t days_until_recent_check = 0;

    if (!http.begin(wifi, RECENT)) return tuple(SaveResult::HttpBeginFailed, 0);
    if (http.GET() != 200) { result = SaveResult::HttpRequestFailed; goto http_end; }
    if (!(stream = http.getStreamPtr())) { result = SaveResult::StreamGetFailed; goto http_end; }

    if (!wait(stream)) { result = SaveResult::Empty; goto http_end; }
    days_until_recent_check = stream->read();
    if (days_until_recent_check == 0 || days_until_recent_check > DAYS_UNTIL_RECENT_CHECK_ERROR) { result = SaveResult::LimitExceded; goto http_end; }
    if (!wait(stream)) { result = SaveResult::Ok; goto stream_stop; }

    result = (SaveResult)download(stream, RECENT_FILE);
    if (result == SaveResult::ClearFailed) goto stream_stop;
    if (result != SaveResult::Ok) goto remove;
    if (stream->available()) { result = SaveResult::WrongLength; goto remove; }

    goto stream_stop;

    remove:
    SD.remove(RECENT_FILE);
    stream_stop:
    stream->stop();
    http_end:
    http.end();
    return tuple(result, result == SaveResult::Ok ? days_until_recent_check : 0);
}
// result, images, min_file_count
tuple<SaveResult, uint8_t, uint8_t> saveRand(const uint8_t next_rand_file) {
    SaveResult result = SaveResult::Ok;
    WiFiClient wifi;
    HTTPClient http;
    WiFiClient *stream;

    uint8_t image_count = 0;
    uint8_t min_file_count = 0;

    if (!http.begin(wifi, RANDOM)) return tuple(SaveResult::HttpBeginFailed, image_count, min_file_count);
    if (http.GET() != 200) { result = SaveResult::HttpRequestFailed; goto http_end; }
    if (!(stream = http.getStreamPtr())) { result = SaveResult::StreamGetFailed; goto http_end; }

    if (!wait(stream)) { result = SaveResult::Empty; goto http_end; }
    min_file_count = stream->read();
    if (min_file_count == 0 || min_file_count > MIN_FILE_COUNT_ERROR) { result = SaveResult::LimitExceded; goto http_end; }

    while (wait(stream)) {
        image_count += 1;
        if (image_count > MAX_SAVED_IMAGE_COUNT || image_count > 255 - next_rand_file) { result = SaveResult::WrongLength; goto remove; }
        const DownloadResult download_result = download(stream, numToName(next_rand_file + image_count).bytes);
        result = download_result == DownloadResult::Ok ? SaveResult::Ok : (SaveResult)download_result;
        if (result == SaveResult::CreateFailed) image_count -= 1;
        if (result != SaveResult::Ok) goto remove;
    }

    goto stream_stop;
    
    remove:
    removeFiles(next_rand_file, next_rand_file + image_count);
    stream_stop:
    stream->stop();
    http_end:
    http.end();
    return result == SaveResult::Ok ? tuple(SaveResult::Ok, image_count, min_file_count) : tuple(result, (uint8_t)0, (uint8_t)0);
}

//while (!wifi.connect("concepts.scienceontheweb.net", 80)) delay(500);
//
//wifi.println("GET / HTTP/1.0");
//wifi.println("Host: concepts.scienceontheweb.net");
//wifi.println("Cache-Control: no-cache");
//
//while (!wifi.available()) delay(500);
//while (wifi.available()) Serial.print(static_cast<char>(wifi.read()));
