#pragma once

#ifdef ESP8266
  #include <NTPClient.h>
  #include <WiFiUdp.h>
  #include <ESP8266WiFi.h>
#elif defined(ESP32)
  #include <WiFi.h>
  #include <time.h>
#endif

#define TIME_STORAGE_ADDRESS 32

#include <EEPROM.h>

#include "Logger.h"
#include "AppState.h"
#include "ConfigSettings.h"

class TimeModule {
public:
    TimeModule(Logger& logger, AppState& appState, Settings& settings);
    void beginNTP();
    void updateTime();
    time_t getCurrentTime();
    char* getFormattedTime(const char* format = "%Y-%m-%d %H:%M:%S");
    bool setTimeManually(const String &date, const String &time);
    String getFormattedDateTime();

    void setTimezone(int8_t timeZoneHours);
    void applyTimezone();

    bool isInternetAvailable = false;

private:
    Logger& logger;
    AppState& appState;
    Settings& settings;

    char* loadTimeFromStorage();
    void saveTimeToStorage(const char* timeStr);

    int16_t getTimezoneOffset();
    const char* getTimezoneString();

#ifdef ESP8266
    WiFiUDP ntpUDP;
    NTPClient timeClient;
#endif

};
