#ifndef LOGGER_H_
#define LOGGER_H_

#include <algorithm>
#include <utility>

#include "CommonTypes.h"

constexpr uint8_t MAX_LOG_MESSAGES = 50;
constexpr size_t MAX_MESSAGE_LENGTH = 128;
constexpr size_t MAX_TIMESTAMP_LENGTH = 20;
constexpr uint16_t MAX_FILE_LOG_LINES = 100;

enum LogType {
    LOG_ERROR = 0,
    LOG_INFO = 1,
    LOG_USER = 2
};

struct LogEntry {
    char message[MAX_MESSAGE_LENGTH];
    char timestamp[MAX_TIMESTAMP_LENGTH];
    bool isSay = false;
    uint8_t typeMsg = LOG_USER;
};

class Logger {
private:

    LogEntry* logList = nullptr;

    uint8_t currentIndex = 0;
    uint8_t logCount = 0;
    bool _loggingEnabled = true;
    bool _isPsramUsed = false;
    std::function<void(const LogEntry&)> _newLogCallback = nullptr;

    void formatTime(char* buffer, size_t bufferSize) const {
        time_t now = time(nullptr);
        if (now > 0) {
            struct tm* timeinfo = localtime(&now);
            if (timeinfo && timeinfo->tm_year > (2023 - 1900)) {
                strftime(buffer, bufferSize, "%d-%m-%Y %H:%M:%S", timeinfo);
                return;
            }
        }
        unsigned long uptimeSeconds = millis() / 1000;
        unsigned long hours = uptimeSeconds / 3600;
        unsigned long minutes = (uptimeSeconds % 3600) / 60;
        unsigned long seconds = uptimeSeconds % 60;
        snprintf(buffer, bufferSize, "Uptime %02lu:%02lu:%02lu", hours, minutes, seconds);
    }

    uint8_t _unsentCount = 0;
    uint8_t _sentSinceLastSave = 0;
    static constexpr uint8_t SAVE_TRIGGER_COUNT = 10;

    void initMemory() {
        size_t totalSize = sizeof(LogEntry) * MAX_LOG_MESSAGES;
        Serial.printf("[LOGGER_DBG] Attempting to allocate %d bytes for log buffer...\n", totalSize);

        if (psramFound()) {
            logList = (LogEntry*) ps_malloc(totalSize);
            if (logList) {
                _isPsramUsed = true;
                Serial.println("[LOGGER_DBG] Successfully allocated log buffer in PSRAM.");
            }
        }

        if (!logList) {
            logList = (LogEntry*) malloc(totalSize);
            if (logList) {
                _isPsramUsed = false;
                Serial.println("[LOGGER_DBG] PSRAM allocation failed. Successfully allocated log buffer in SRAM.");
            } else {
                Serial.println("[LOGGER_DBG] CRITICAL: Failed to allocate memory for log buffer in both PSRAM and SRAM. Logger will be disabled.");
                _loggingEnabled = false;
            }
        }

        if (logList) {
            memset(logList, 0, totalSize);
        }
    }

public:

    Logger() {
        initMemory();
    }

    ~Logger() {
        if (logList) {
            free(logList);
            logList = nullptr;
            Serial.println("[LOGGER_DBG] Log buffer memory freed.");
        }
    }

    uint8_t getUnsentCount() const { return _unsentCount; }
    uint8_t getSentSinceLastSave() const { return _sentSinceLastSave; }
    bool isPsramUsed() const { return _isPsramUsed; }

    void forceSave() {
        #ifdef LOGGER_DEBUG
        Serial.println("[LOGGER_DBG] Force save triggered.");
        #endif
        saveLogsToSPIFFS();
        _sentSinceLastSave = 0;
    }

    void setNewLogCallback(std::function<void(const LogEntry&)> callback) {
        _newLogCallback = callback;
    }

    void removeNewLogCallback() {
        _newLogCallback = nullptr;
    }

    void setLoggingEnabled(bool enabled) {
        _loggingEnabled = enabled && (logList != nullptr);
        #ifdef LOGGER_DEBUG
        Serial.println("[LOGGER_DBG] Logging state set to: " + String(_loggingEnabled ? "ENABLED" : "DISABLED"));
        #endif
    }

    bool isLoggingEnabled() const {
        return _loggingEnabled;
    }

    void addLog(const String& message, uint8_t typeMsg = LOG_INFO) {
        if (!_loggingEnabled || !logList) return;

        if (logCount >= MAX_LOG_MESSAGES) {
            if (!logList[currentIndex].isSay) {
                _unsentCount--;
            }
        }

        LogEntry& newEntry = logList[currentIndex];

        char timeBuffer[MAX_TIMESTAMP_LENGTH];
        formatTime(timeBuffer, sizeof(timeBuffer));

        strncpy(newEntry.timestamp, timeBuffer, MAX_TIMESTAMP_LENGTH - 1);
        newEntry.timestamp[MAX_TIMESTAMP_LENGTH - 1] = '\0';

        strncpy(newEntry.message, message.c_str(), MAX_MESSAGE_LENGTH - 1);
        newEntry.message[MAX_MESSAGE_LENGTH - 1] = '\0';

        newEntry.isSay = false;
        newEntry.typeMsg = (typeMsg <= LOG_USER) ? typeMsg : LOG_USER;

        #ifdef LOGGER_DEBUG
        Serial.printf("[LOG] Add: idx=%d, type=%d, isSay=%d, msg='%s'\n",
                     currentIndex, typeMsg, newEntry.isSay, newEntry.message);
        #endif

        if (logCount < MAX_LOG_MESSAGES) {
            logCount++;
        }
        currentIndex = (currentIndex + 1) % MAX_LOG_MESSAGES;
        _unsentCount++;

        if (_newLogCallback) {
            _newLogCallback(newEntry);
        }
    }

    LogEntry* getLogEntry(uint8_t index) {
        if (index >= logCount || !logList) return nullptr;

        uint8_t bufferIndex;
        if (logCount < MAX_LOG_MESSAGES) {
            bufferIndex = index;
        } else {
            bufferIndex = (currentIndex + index) % MAX_LOG_MESSAGES;
        }

        return &logList[bufferIndex];
    }

    bool markAsSent(uint8_t bufferIndex) {
        if (bufferIndex < MAX_LOG_MESSAGES && logList) {
            if (!logList[bufferIndex].isSay) {
                logList[bufferIndex].isSay = true;

                _unsentCount--;
                _sentSinceLastSave++;

                #ifdef LOGGER_DEBUG
                Serial.printf("[LOG] Marked as sent: bufferIdx=%d. Unsent left: %d, Sent since save: %d\n",
                             bufferIndex, _unsentCount, _sentSinceLastSave);
                #endif

                if (_sentSinceLastSave >= SAVE_TRIGGER_COUNT) {
                    Serial.printf("[LOGGER_DBG] %d messages sent, triggering save to SPIFFS.\n", _sentSinceLastSave);
                    saveLogsToSPIFFS();
                }
            }
            return true;
        }
        return false;
    }

    uint8_t getUnsentMessages(std::vector<std::pair<LogEntry*, uint8_t>>& result, uint8_t maxCount) {
        result.clear();
        if (logCount == 0 || !logList) return 0;

        uint8_t startIdx;
        if (logCount < MAX_LOG_MESSAGES) {
            startIdx = 0;
        } else {
            startIdx = currentIndex;
        }

        for (size_t i = 0; i < logCount && result.size() < maxCount; ++i) {
            uint8_t idx = (startIdx + i) % MAX_LOG_MESSAGES;
            if (!logList[idx].isSay) {
                result.push_back({&logList[idx], idx});
            }
        }
        return result.size();
    }

    void saveLogsToSPIFFS() {
        if (!logList) return;

        const char* filename = "/log.txt";
        const char* tempFilename = "/log.tmp";

        File originalFile = SPIFFS.open(filename, FILE_READ);
        File tempFile = SPIFFS.open(tempFilename, FILE_WRITE);
        if (!originalFile || !tempFile) {
            Serial.println("Ошибка открытия файлов для ротации");
            if (originalFile) originalFile.close();
            if (tempFile) tempFile.close();
            return;
        }

        std::vector<String> oldLines;
        while (originalFile.available()) {
            String line = originalFile.readStringUntil('\n');
            line.trim();
            if (line.length() > 0) {
                oldLines.push_back(line);
            }
              yield();
        }
        originalFile.close();

        int linesToKeep = MAX_FILE_LOG_LINES - logCount;
        if (linesToKeep > 0 && oldLines.size() > 0) {
            auto start_it = oldLines.end() - min(linesToKeep, (int)oldLines.size());
            for (auto it = start_it; it != oldLines.end(); ++it) {
                tempFile.println(*it);
            }
        }

        if (oldLines.size() >= MAX_FILE_LOG_LINES) {
            tempFile.println("[]==================[]=================[]\n");
        }

        uint8_t startIdx = (logCount < MAX_LOG_MESSAGES) ? 0 : currentIndex;
        for (size_t i = 0; i < logCount; ++i) {
            uint8_t idx = (startIdx + i) % MAX_LOG_MESSAGES;

            char logBuffer[MAX_MESSAGE_LENGTH + MAX_TIMESTAMP_LENGTH + 10];
            snprintf(logBuffer, sizeof(logBuffer), "%s;%c;%d;%s",
                     logList[idx].timestamp,
                     logList[idx].isSay ? '1' : '0',
                     logList[idx].typeMsg,
                     logList[idx].message);
            tempFile.println(logBuffer);

           if (i % 20 == 0) {
            yield();
            delay(1);
          }
        }

        tempFile.close();
        SPIFFS.remove(filename);
        SPIFFS.rename(tempFilename, filename);

        _sentSinceLastSave = 0;

        #ifdef LOGGER_DEBUG
        Serial.println("[LOGGER_DBG] Save complete. Resetting sent-since-save counter to 0.");
        #endif
    }

    void loadLogsFromSPIFFS() {
        if (!logList) return;

        const char* filename = "/log.txt";

        if (!SPIFFS.exists(filename)) {
            Serial.println("Файл лога не найден.");
            return;
        }

        File file = SPIFFS.open(filename, FILE_READ);
        if (!file) {
            Serial.println("Ошибка открытия файла лога для чтения.");
            return;
        }

        _unsentCount = 0;

        uint8_t loadedCount = 0;
        while (file.available() && logCount < MAX_LOG_MESSAGES) {
            String line = file.readStringUntil('\n');
            line.trim();
            if (line.length() == 0) continue;

            int separator1 = line.indexOf(';');
            int separator2 = line.indexOf(';', separator1 + 1);
            int separator3 = line.indexOf(';', separator2 + 1);

            if (separator1 != -1 && separator2 != -1 && separator3 != -1 &&
                separator3 > separator2 && separator2 > separator1) {

                strncpy(logList[currentIndex].timestamp, line.substring(0, separator1).c_str(), MAX_TIMESTAMP_LENGTH - 1);
                logList[currentIndex].timestamp[MAX_TIMESTAMP_LENGTH - 1] = '\0';

                logList[currentIndex].isSay = line.substring(separator1 + 1, separator2).equalsIgnoreCase("1");

                String typeStr = line.substring(separator2 + 1, separator3);
                logList[currentIndex].typeMsg = typeStr.toInt();
                if (logList[currentIndex].typeMsg > LOG_USER) {
                    logList[currentIndex].typeMsg = LOG_USER;
                }

                strncpy(logList[currentIndex].message, line.substring(separator3 + 1).c_str(), MAX_MESSAGE_LENGTH - 1);
                logList[currentIndex].message[MAX_MESSAGE_LENGTH - 1] = '\0';

                if (!logList[currentIndex].isSay) {
                    _unsentCount++;
                }

                currentIndex = (currentIndex + 1) % MAX_LOG_MESSAGES;
                if (logCount < MAX_LOG_MESSAGES) {
                    logCount++;
                }
                loadedCount++;
            }
        }

        file.close();
        Serial.println("Логи загружены из SPIFFS. Новых записей: " + String(loadedCount) + ", всего в памяти: " + String(logCount));
    }

    uint8_t getLogCount() const { return logCount; }

    String getAllLogsJSON() const {

        StaticJsonDocument<8192> jsonDoc;
        JsonArray logsArray = jsonDoc.to<JsonArray>();

        if (logCount == 0 || !logList) {
            String output;
            serializeJson(jsonDoc, output);
            return output;
        }

        uint8_t startIdx;
        if (logCount < MAX_LOG_MESSAGES) {
            startIdx = 0;
        } else {
            startIdx = currentIndex;
        }

        for (size_t i = 0; i < logCount; ++i) {
            uint8_t idx = (startIdx + i) % MAX_LOG_MESSAGES;
            JsonObject logEntry = logsArray.createNestedObject();
            logEntry["timestamp"] = logList[idx].timestamp;
            logEntry["message"] = logList[idx].message;
            logEntry["isSay"] = logList[idx].isSay;
            logEntry["typeMsg"] = logList[idx].typeMsg;

             if (i % 30 == 0) yield();
        }

        String output;
        serializeJson(jsonDoc, output);
        return output;
    }

    void clearLogs() {
        if (!logList) return;

        for (uint8_t i = 0; i < logCount; ++i) {

            memset(logList[i].message, 0, MAX_MESSAGE_LENGTH);
            memset(logList[i].timestamp, 0, MAX_TIMESTAMP_LENGTH);
            logList[i].isSay = false;
            logList[i].typeMsg = LOG_USER;
        }

        logCount = 0;
        currentIndex = 0;
        _unsentCount = 0;
        _sentSinceLastSave = 0;

        const char* filename = "/log.txt";
        File file = SPIFFS.open(filename, FILE_WRITE);
        if (!file) {
            Serial.println("Не удалось открыть файл лога для очистки.");
            return;
        }
        file.close();
    }

    std::vector<LogEntry*> getLogsByType(uint8_t type, uint8_t maxCount = 10) {
        std::vector<LogEntry*> result;
        if (logCount == 0 || type > LOG_USER || !logList) return result;

        uint8_t startIdx;
        if (logCount < MAX_LOG_MESSAGES) {
            startIdx = 0;
        } else {
            startIdx = currentIndex;
        }

        for (size_t i = 0; i < logCount && result.size() < maxCount; ++i) {
            uint8_t idx = (startIdx + i) % MAX_LOG_MESSAGES;
            if (logList[idx].typeMsg == type) {
                result.push_back(&logList[idx]);
            }
        }
        return result;
    }
};

#endif
