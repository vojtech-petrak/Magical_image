#ifndef STORAGE_H
#define STORAGE_H

#include <cstdint>
#include <pins_arduino.h>
#include <tuple>

//enum class Result : uint8_t {
//    Ok,
//    RemoveFailed = 1,
//    RenameFailed = 2,
//    WrongLength = 3,
//    TooLarge = 4,
//    TooShort = 5,
//    Empty = 6,
//    DoesNotExist = 7,
//
//    RecentWriteFailed = 8,
//    RecentClearFailed = 9,
//    RecentCreationFailed = 10,
//    RecentImageTooLarge = 11,
//    RecentStreamReadFailed = 12,
//    RecentStreamTooShort = 13,
//    RecentStreamNotConnected = 14,
//    RecentStreamTooLong = 15,
//    RecentStreamGetFailed = 16,
//    RecentHttpRequestFailed = 17,
//    RecentHttpBeginFailed = 18,
//
//    RandWriteFailed = 19,
//    RandClearFailed = 20,
//    RandCreationFailed = 21,
//    RandImageTooLarge = 22,
//    RandStreamReadFailed = 23,
//    RandStreamTooShort = 24,
//    RandStreamNotConnected = 25,
//    RandStreamTooLong = 26,
//    RandStreamGetFailed = 27,
//    RandHttpRequestFailed = 28,
//    RandHttpBeginFailed = 29,
//
//    HttpBeginFailed = 30,
//    HttpRequestFailed = 31,
//    NtpUpdateFailed = 32,
//    TimeLost = 33,
//    SdNotConnected = 34,
//};
enum class Result : uint8_t {
    Ok,

    RemoveFailed = 1,
    RenameFailed = 2,
    WriteFailed = 3,
    ClearFailed = 4,
    CreateFailed = 5,
    ReadOpenFailed = 6,
    WriteOpenFailed = 7,
    FilesMissing = 8,

    StreamReadFailed = 9,
    StreamNotConnected = 10,
    LimitExceded = 11,
    StreamGetFailed = 12,
    HttpRequestFailed = 13,
    HttpBeginFailed = 14,

    WrongLength = 15,
    TooLarge = 16,
    TooShort = 17,
    Empty = 18,

    RtcWriteFailed = 19,
    RtcReadFailed = 20,

    NtpUpdateFailed = 21,
    TimeLost = 22,
    SdNotConnected = 23,
};
enum class Type : uint8_t {
    Generic = 0x00,
    DayGeneric = 0x20,
    DayRecent = 0x40,
    DayRand = 0x60,
    NightGeneric = 0x80,
    NightRecent = 0xa0,
    NightRand = 0xc0,
};

class FullResult {
    public:
        FullResult() {}
        FullResult(const uint8_t value) : value(value) {}
        FullResult(const Type type, const Result result) : value((uint8_t)type | (uint8_t)result) {}
        uint8_t value;
};

typedef struct { const char bytes[3]; } RandName;
enum class RandFilesResult : uint8_t {
    Ok,
    RemoveFailed = (uint8_t)Result::RemoveFailed,
    RenameFailed = (uint8_t)Result::RenameFailed,
};

const uint8_t ERROR_BUFFER_SIZE = 16;
const uint8_t ERROR_COUNT_ADDRESS = 16;
const uint8_t EPD_CLEARED_ADDRESS = 17;

const uint8_t MIN_FILE_COUNT_ERROR = 128;
const uint8_t MIN_FILE_COUNT_WARNING = 64;
const uint8_t DAYS_UNTIL_RECENT_CHECK_ERROR = 64;
const uint8_t DAYS_UNTIL_RECENT_CHECK_WARNING = 32;
const uint8_t MAX_SAVED_IMAGE_COUNT = 64;

const uint8_t DAYS_UNTIL_BATTERY_CHECK = 7;
const uint8_t BATTERY_CHARGE_WARNING = 102;
const uint8_t BATTERY_CHARGE_ERROR = 51;

const uint8_t DEFAULT_MIN_FILE_COUNT = 30;
const uint8_t MAX_FAILED_WIFI_CONNECTIONS = 3;

const uint8_t SD_CS = D8;
const char STATE_FILE[] = "state";
const char RECENT_FILE[] = "recent";

RandName numToName(const uint8_t num);
std::tuple<bool, uint8_t> randFileCount();
RandFilesResult removeFiles(const uint8_t first_inclusive, const uint8_t last_exclusive);
//RandFilesResult shiftFiles(const uint8_t rand_file_count);
//RandFilesResult rotateFiles(const uint8_t rand_file_count);
RandFilesResult shiftFiles(const uint8_t rand_file_count, const uint8_t n);

void writeError(uint8_t *const error_count, const FullResult error);
void writeError(FullResult *const errors, uint8_t *const error_count, const FullResult error);
void writeError(uint8_t *const error_count, const Type type, const Result error);
void writeError(FullResult *const errors, uint8_t *const error_count, const Type type, const Result error);

#endif // !STORAGE_H
