// original code: https://github.com/waveshareteam/e-Paper/tree/master/Arduino/epd5in65f
// manul: https://www.waveshare.com/wiki/5.65inch_e-Paper_Module_(F)_Manual
// commands: https://files.waveshare.com/upload/7/7a/5.65inch_e-Paper_%28F%29_Sepecification.pdf

#include "epd.h"
#include "storage.h"
#include <SD.h>
#include <SPI.h>
#include <EEPROM.h>
#include <user_interface.h>

using namespace std;

// public:

Epd::Epd() {
    //pinMode(PWR_PIN, OUTPUT);
    pinMode(BUSY_PIN, INPUT); 
    pinMode(RST_PIN, OUTPUT);
    pinMode(DC_PIN, OUTPUT);
    //pinMode(CS_PIN, OUTPUT);

    //digitalWrite(PWR_PIN, HIGH);
    //delay(500);

    digitalWrite(CS_PIN, LOW);

    SPI.begin();
    SPI.beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE0));

    reset();
    busyHigh();

    sendCommand(0x00); // 1 panel setting
    sendData(0xEF);
    sendData(0x08);

    sendCommand(0x01); // 2 power setting
    sendData(0x37);
    sendData(0x00);
    sendData(0x23);
    sendData(0x23);

    sendCommand(0x03); // 4 power off sequence setting
    sendData(0x00);

    sendCommand(0x06); // 6 booster soft start / starting data transmission
    sendData(0xC7);
    sendData(0xC7);
    sendData(0x1D);

    sendCommand(0x30); // 12 PPL control
    sendData(0x3C);

    sendCommand(0x41); // 14 temperature sensor enable
    sendData(0x00);

    sendCommand(0x50); // 17 VCOM and data interval setting
    sendData(0x37);

    sendCommand(0x60);
    sendData(0x22);

    setResolution();

    sendCommand(0xE3);
    sendData(0xAA);
        
    delay(100);
    sendCommand(0x50);
    sendData(0x37);

    digitalWrite(CS_PIN, HIGH);
}
Epd::~Epd() {
    digitalWrite(CS_PIN, LOW);
    sendCommand(0x07);
    sendData(0xA5);
    digitalWrite(CS_PIN, HIGH);

    delay(100);
    digitalWrite(RST_PIN, LOW);
}
void Epd::reset() {
    digitalWrite(RST_PIN, LOW);
    delay(1);
    digitalWrite(RST_PIN, HIGH);
    delay(200);    
}

void Epd::clear(const Color color) {
    digitalWrite(CS_PIN, LOW);
    setResolution();
    startImageTransfer();
    for (uint32_t i = 0; i < (uint32_t)WIDTH * (uint32_t)HEIGHT; i += 1) {
        SPI.transfer(((uint8_t)color << 4) | (uint8_t)color);
    }
    refresh();
    digitalWrite(CS_PIN, HIGH);
}
DisplayResult Epd::displayFile(File file) {
    if (!file.available()) return DisplayResult::Empty;
    if (file.available() < 2) return DisplayResult::TooShort;
    const uint16_t image_width = file.read() | (file.read() << 8);
    if (image_width > WIDTH) return DisplayResult::TooLarge;
    if (file.available() % image_width != 0) return DisplayResult::WrongLength;
    const uint16_t image_height = file.available() / image_width;
    if (image_height > HEIGHT) return DisplayResult::TooLarge;
    const uint16_t image_x = (WIDTH - image_width) / 2;
    const uint16_t image_y = (HEIGHT - image_height) / 2;

    digitalWrite(CS_PIN, LOW);
    setResolution();
    startImageTransfer();

    for (uint16_t y = 0; y < HEIGHT; y += 1) for (uint16_t x = 0; x < WIDTH; x += 1) {
        uint8_t byte;
        if (y >= image_y && y < image_y + image_height && x >= image_x && x < image_x + image_width) {
            digitalWrite(CS_PIN, HIGH);
            byte = file.read();
            digitalWrite(CS_PIN, LOW);
        } else byte = 0x11;
        SPI.transfer(byte);
    }

    refresh();
    digitalWrite(CS_PIN, HIGH);
    return DisplayResult::Ok;
}

// private:

void Epd::busyHigh() {
    for (uint8_t t = 0; t < 60; t += 1) {
        delay(500);
        if (digitalRead(BUSY_PIN)) break;
    }
}
//void Epd::busyLow() {
//    for (uint8_t t = 0; t < 60; t += 1) {
//        delay(500);
//        if (!digitalRead(BUSY_PIN)) break;
//    }
//}

void Epd::commandMode() {
    digitalWrite(DC_PIN, LOW);
}
void Epd::dataMode() {
    digitalWrite(DC_PIN, HIGH);
}
void Epd::sendCommand(const uint8_t command) {
    commandMode();
    SPI.transfer(command);
}
void Epd::sendData(const uint8_t data) {
    dataMode();
    SPI.transfer(data);
}

void Epd::setResolution() {
    sendCommand(0x61); // 19 resolution setting
    dataMode();
    SPI.transfer(0x02);
    SPI.transfer(0x58);
    SPI.transfer(0x01);
    SPI.transfer(0xC0);
}
void Epd::startImageTransfer() {
    sendCommand(0x10); // 8 start data transmission
    dataMode();
}
void Epd::refresh() {
    commandMode();
    SPI.transfer(0x04); // 5 power on
    busyHigh();
    SPI.transfer(0x12); // 10 display refresh
    busyHigh();
    SPI.transfer(0x02); // 3 power off
    //busyLow();
    delay(1);
}

// non class:

// rand_file_count, next_image, rollover
tuple<uint8_t, uint8_t, bool> night(const uint8_t next_image, uint8_t *const error_count) {
    Epd epd;
    const auto [count_success, next_rand_file] = randFileCount();
    if (!count_success) writeError(error_count, Type::NightGeneric, Result::FilesMissing);

    yield();
    File file = SD.open(RECENT_FILE, FILE_READ);
    if (file) {
        const DisplayResult recent_result = epd.displayFile(file);
        file.close();

        if (!(file = SD.open(RECENT_FILE, FILE_WRITE))) writeError(error_count, Type::NightRecent, Result::WriteOpenFailed);
        else if (!file.truncate(0)) writeError(error_count, Type::NightRecent, Result::ClearFailed);
        file.close();

        if (recent_result == DisplayResult::Ok) {
            if (EEPROM.read(EPD_CLEARED_ADDRESS)) EEPROM.write(EPD_CLEARED_ADDRESS, 0);
            return tuple(next_rand_file, next_image, false);
        }
        else if (recent_result != DisplayResult::Empty) writeError(error_count, Type::NightRecent, (Result)recent_result);
    } else {
        writeError(error_count, Type::NightRecent, Result::ReadOpenFailed);
        if ((file = SD.open(RECENT_FILE, FILE_WRITE))) file.close();
        else writeError(error_count, Type::NightRecent, Result::CreateFailed);
    }

    if (next_rand_file == 0) return tuple(0, 0, false);

    uint8_t rand_file_count = next_rand_file;
    uint8_t new_next_image = next_image;
    while (true) {
        yield();
        if ((file = SD.open(numToName(new_next_image++).bytes))) {
            const DisplayResult result = epd.displayFile(file);
            file.close();
            if (result == DisplayResult::Ok) {
                if (EEPROM.read(EPD_CLEARED_ADDRESS)) EEPROM.write(EPD_CLEARED_ADDRESS, 0);
                break;
            }
            writeError(error_count, Type::NightRand, (Result)result);
            rand_file_count -= 1;
        } else {
            writeError(error_count, Type::NightRand, Result::ReadOpenFailed);
            if (new_next_image >= next_rand_file) new_next_image = 0;
        }

        if (rand_file_count == 0 || new_next_image == next_image) {
            if (!EEPROM.read(EPD_CLEARED_ADDRESS)) {
                epd.clear(Color::White);
                EEPROM.write(EPD_CLEARED_ADDRESS, 1);
            }
            break;
        }
    }

    return tuple(rand_file_count, new_next_image, new_next_image <= next_image);
}

void clearEpd() {
    if (EEPROM.read(EPD_CLEARED_ADDRESS)) return;
    Epd epd;
    epd.clear(Color::White);
    EEPROM.write(EPD_CLEARED_ADDRESS, 1);
}
