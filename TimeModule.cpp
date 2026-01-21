#include "TimeModule.h"

TimeModule::TimeModule(Logger& logger, AppState& appState, Settings& settings)
: logger(logger),
  appState(appState),
  settings(settings)
#ifdef ESP8266
  , timeClient(ntpUDP, "pool.ntp.org", getTimezoneOffset(), 5000)
#endif
{
    applyTimezone();
}

void TimeModule::beginNTP() {
#ifdef ESP8266
    timeClient.begin();
#endif
}

time_t TimeModule::getCurrentTime() {
    return time(nullptr);
}

int16_t TimeModule::getTimezoneOffset() {
    return settings.ws.timeZone * 3600;
}

const char* TimeModule::getTimezoneString() {

    static char tzStr[16];
    if (settings.ws.timeZone >= 0) {
        snprintf(tzStr, sizeof(tzStr), "UTC+%d", settings.ws.timeZone);
    } else {
        snprintf(tzStr, sizeof(tzStr), "UTC%d", settings.ws.timeZone);
    }
    return tzStr;
}

void TimeModule::applyTimezone() {

    setenv("TZ", getTimezoneString(), 1);
    tzset();

#ifdef ESP8266
    timeClient.setTimeOffset(getTimezoneOffset());
#endif

    Serial.printf("Timezone set to: %s (%d hours)\n",
                  getTimezoneString(), settings.ws.timeZone);
}

void TimeModule::setTimezone(int8_t timeZoneHours) {

    if (timeZoneHours >= -12 && timeZoneHours <= 14) {
        settings.ws.timeZone = timeZoneHours;
        applyTimezone();
    }
}

char* TimeModule::getFormattedTime(const char* format) {
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    static char buffer[64];
    strftime(buffer, sizeof(buffer), format, &timeinfo);
    return buffer;
}

String TimeModule::getFormattedDateTime() {
    return String(getFormattedTime());
}

void TimeModule::updateTime() {
    Serial.println("=== updateTime() started ===");
    applyTimezone();

    time_t currentSystemTime = time(nullptr);
    struct tm timeinfo;
    localtime_r(&currentSystemTime, &timeinfo);

    int year = timeinfo.tm_year + 1900;

    bool isSystemTimeCompletelyInvalid = (year < 2024);

    time_t storedTime = 0;
    char* loadedTimeStr = loadTimeFromStorage();
    if (loadedTimeStr) {
        struct tm storedTm = {};
        if (strptime(loadedTimeStr, "%Y-%m-%d %H:%M:%S", &storedTm)) {
            storedTime = mktime(&storedTm);
        }
        free(loadedTimeStr);
    }

    bool shouldUseStoredTime = false;

    if (isSystemTimeCompletelyInvalid) {

        Serial.println("System time is invalid. Attempting to restore from storage.");
        shouldUseStoredTime = true;
    } else if (storedTime > 0) {

        const int TIME_DIFF_THRESHOLD_SECONDS = 300;

        if (storedTime > (currentSystemTime + TIME_DIFF_THRESHOLD_SECONDS)) {
            Serial.printf("Stored time is significantly newer than RTC. Restoring from storage.\n");
            shouldUseStoredTime = true;
        }
    }

    if (shouldUseStoredTime && storedTime > 0) {

        struct timeval tv = { .tv_sec = storedTime, .tv_usec = 0 };
        settimeofday(&tv, NULL);
        Serial.printf("Time restored from storage: %s", ctime(&storedTime));
    } else if (isSystemTimeCompletelyInvalid) {

        Serial.println("System time invalid and no valid stored time. Setting emergency time.");
        struct tm tmp = {};
        tmp.tm_year = 2024 - 1900;
        tmp.tm_mon = 0;
        tmp.tm_mday = 1;
        tmp.tm_hour = 12;
        time_t emergencyTime = mktime(&tmp);
        struct timeval tv = { .tv_sec = emergencyTime, .tv_usec = 0 };
        settimeofday(&tv, NULL);
    }

    if (WiFi.isConnected() && WiFi.getMode() == WIFI_STA) {
        bool ntpUpdated = false;
#ifdef ESP8266
        timeClient.setTimeOffset(getTimezoneOffset());
        ntpUpdated = timeClient.forceUpdate();
        if (ntpUpdated) currentSystemTime = timeClient.getEpochTime();
#else
        configTime(getTimezoneOffset(), 0, "pool.ntp.org", "time.nist.gov");
        if (getLocalTime(&timeinfo, 5000)) {
            currentSystemTime = mktime(&timeinfo);
            ntpUpdated = true;
        }
#endif
        if (ntpUpdated) {
            struct timeval tv = { .tv_sec = currentSystemTime, .tv_usec = 0 };
            settimeofday(&tv, NULL);
            char timeStr[64];
            localtime_r(&currentSystemTime, &timeinfo);
            strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
            saveTimeToStorage(timeStr);
            isInternetAvailable = true;
            Serial.printf("NTP sync OK: %ld\n", currentSystemTime);
        } else {
            Serial.println("NTP update failed");
            isInternetAvailable = false;
        }
    }

    time_t finalTime = time(nullptr);
    if (localtime_r(&finalTime, &timeinfo)) {
        char finalTimeStr[64];
        strftime(finalTimeStr, sizeof(finalTimeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
        Serial.printf("Final system time: %s\n", finalTimeStr);
    }

    Serial.println("=== updateTime() finished ===");
}

char* TimeModule::loadTimeFromStorage() {

    char buffer[64] = {0};
    for (int i = 0; i < 63; i++) {

        buffer[i] = EEPROM.read(TIME_STORAGE_ADDRESS + i);
        if (buffer[i] == '\0') break;
    }

    if (strlen(buffer) > 0) return strdup(buffer);
    return NULL;
}

void TimeModule::saveTimeToStorage(const char* timeStr) {

    char storedTime[64] = {0};
    for (int i = 0; i < 63; i++) {
        storedTime[i] = EEPROM.read(TIME_STORAGE_ADDRESS + i);
        if (storedTime[i] == '\0') break;
    }

    if (strcmp(storedTime, timeStr) == 0) {
        return;
    }

    Serial.printf("Time changed, writing new time to EEPROM: %s\n", timeStr);
    for (int i = 0; i < strlen(timeStr); i++) {
        EEPROM.write(TIME_STORAGE_ADDRESS + i, timeStr[i]);
    }

    EEPROM.write(TIME_STORAGE_ADDRESS + strlen(timeStr), '\0');
    EEPROM.commit();
}

  bool TimeModule::setTimeManually(const String &date, const String &time) {

    if (date.length() < 10 || time.length() < 5) {
      Serial.println("Слишком короткая дата или время");
      return false;
    }

    int year = date.substring(0, 4).toInt();
    int month = date.substring(5, 7).toInt();
    int day = date.substring(8, 10).toInt();

    int hour = time.substring(0, 2).toInt();
    int minute = time.substring(3, 5).toInt();
    int second = (time.length() == 8) ? time.substring(6, 8).toInt() : 0;

    if (year < 2000 || year > 9100 || month < 1 || month > 12 || day < 1 || day > 31 ||
        hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59) {
      Serial.println("Ошибка: неверные значения даты или времени");
      return false;
    }

    struct tm newTime = {};
    newTime.tm_year = year - 1900;
    newTime.tm_mon = month - 1;
    newTime.tm_mday = day;
    newTime.tm_hour = hour;
    newTime.tm_min = minute;
    newTime.tm_sec = second;

    time_t newEpoch = mktime(&newTime);

    struct timeval tv = { .tv_sec = newEpoch, .tv_usec = 0 };
    if (settimeofday(&tv, NULL) == 0) {
      Serial.println("Время успешно установлено");
    } else {
      Serial.println("Ошибка при установке времени");
      return false;
    }

    struct tm timeinfo;
    if (!localtime_r(&newEpoch, &timeinfo)) return false;
    char timeStr[64];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
    saveTimeToStorage(timeStr);

    Serial.print("Установлено время: ");
    Serial.println(timeStr);
    return true;
  }
