#ifndef DOWNLOAD_H
#define DOWNLOAD_H

#include "storage.h"
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266HTTPClient.h>

enum class DownloadResult : uint8_t {
    Ok,

    WriteFailed        = (uint8_t)Result::WriteFailed,
    ClearFailed        = (uint8_t)Result::ClearFailed,
    CreateFailed       = (uint8_t)Result::CreateFailed,

    StreamReadFailed   = (uint8_t)Result::StreamReadFailed,
    StreamNotConnected = (uint8_t)Result::StreamNotConnected,

    TooLarge           = (uint8_t)Result::TooLarge,
    TooShort           = (uint8_t)Result::TooShort,
};
enum class SaveResult : uint8_t {
    Ok,

    WriteFailed        = (uint8_t)Result::WriteFailed,
    ClearFailed        = (uint8_t)Result::ClearFailed,
    CreateFailed       = (uint8_t)Result::CreateFailed,

    StreamReadFailed   = (uint8_t)Result::StreamReadFailed,
    StreamNotConnected = (uint8_t)Result::StreamNotConnected,
    LimitExceded       = (uint8_t)Result::LimitExceded,
    StreamGetFailed    = (uint8_t)Result::StreamGetFailed,
    HttpRequestFailed  = (uint8_t)Result::HttpRequestFailed,
    HttpBeginFailed    = (uint8_t)Result::HttpBeginFailed,

    WrongLength        = (uint8_t)Result::WrongLength,
    TooLarge           = (uint8_t)Result::TooLarge,
    TooShort           = (uint8_t)Result::TooShort,
    Empty              = (uint8_t)Result::Empty,
};
enum class ReportResult : uint8_t {
    Ok,
    HttpBeginFailed   = (uint8_t)Result::HttpBeginFailed,
    HttpRequestFailed = (uint8_t)Result::HttpRequestFailed,
};

const char SSID[] = "your service set identifier";
const char PSW[] = "your password";
const char REPORT[] = "your error and battery report server url";
const char RECENT[] = "your recent file server url";
const char RANDOM[] = "your random random file server url";

void disconnectWifi();
ReportResult reportBattery(const uint8_t charge);
ReportResult reportErrors(const FullResult *const errors, const uint8_t error_count);
uint8_t getNtpHour();
// result, days_until_current_check
std::tuple<SaveResult, uint8_t> saveRecent();
// result, image_count, min_file_count
std::tuple<SaveResult, uint8_t, uint8_t> saveRand(const uint8_t next_rand_file);

#endif // !DOWNLOAD_H
