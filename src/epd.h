#ifndef EPD_H
#define EPD_H

#include "storage.h"
#include <FS.h>
#include <cstdint>
#include <pins_arduino.h>
#include <tuple>

enum class DisplayResult : uint8_t {
    Ok,

    WrongLength = (uint8_t)Result::WrongLength,
    TooLarge    = (uint8_t)Result::TooLarge,
    TooShort    = (uint8_t)Result::TooShort,
    Empty       = (uint8_t)Result::Empty,
};

enum class Color : uint8_t {
    Black,
    White,
    Green,
    Blue,
    Red,
    Yellow,
    Orange,
    Clean,
};

// Electronic Paper Display
class Epd {
    public:
        const static uint16_t WIDTH  = 300; // this value is in bytes, 1 byte contains a pair of pixels
        const static uint16_t HEIGHT = 448;
        const static uint8_t CS_PIN  = D4;

        Epd();
        ~Epd();
        void reset();

        void clear(const Color color);
        DisplayResult displayFile(File file);
        //DisplayResult displayRecentFile(const char *const file_name);
        //DisplayResult displayRandFile(const char *const file_name);

    private:
        //const static uint8_t PWR_PIN  = D0;
        const static uint8_t BUSY_PIN = D1;
        const static uint8_t RST_PIN  = D2;
        const static uint8_t DC_PIN   = D3;
        
        void busyHigh();
        //void busyLow();

        void commandMode();
        void dataMode();
        void sendCommand(const uint8_t command);
        void sendData(const uint8_t data);

        void setResolution();
        void startImageTransfer();
        void refresh();
};

// rand_file_count, next_image, rollover
std::tuple<uint8_t, uint8_t, bool> night(const uint8_t next_image, uint8_t *const error_count);
void clearEpd();

#endif // !EPD_H
