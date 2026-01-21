#ifndef TELEGRAMBOT__H
#define TELEGRAMBOT__H

#include "CommonTypes.h"
#include <AsyncTelegram2.h>
#include <ArduinoJson.h>
#include "ConfigSettings.h"
#include "WebServer.h"
#include "Logger.h"
#include "AppState.h"
#include "Ota.h"
#include "DeviceManager.h"
#include "Info.h"

#define CANCEL  "CANCEL"
#define CONFIRM "FLASH_FW"

class TelegramBot
{
public:
    TelegramBot(Settings& ws, WebServer& webServer, Logger& logger, AppState& appState, Ota& ota, Info& sysInfo, DeviceManager& deviceManager);
    void begin();
    void loop();
    void checkMemory();
    bool isValidTokenFormat(const String& token);

    AsyncTelegram2 myBot;

private:
    Settings& settings;
    WebServer& webServer;
    Logger& logger;
    AppState& appState;
    Ota& ota;
    Info& sysInfo;
    DeviceManager& deviceManager;

    unsigned long lastLogCheckTime = 0;
    const unsigned long LOG_CHECK_INTERVAL = 5000;

    bool areCommandsUpToDate();
    void updateBotCommandsOnServer(bool start = false);
    bool commandsNeedUpdate = false;
    bool isFirstStart = true;

    void checkAndSendLogs();
    bool sendLogMessage(const LogEntry& logEntry, int64_t chatId);
    bool shouldSendLog(const LogEntry& logEntry);
    bool shouldSendLogToUser(int64_t chatId);

    void sendSimpleStatus(int64_t chatId);
    void sendHelpMessage(int64_t chatId);
    void handleRelayCommand(int64_t chatId, const String& command);
    void handleSystemToggleCommand(int64_t chatId, const String& command);
    bool hasPermission(const String& userId, const String& permission);
    void sendDocument(TBMessage &msg, AsyncTelegram2::DocumentType fileType, const char* filename, const char* caption = nullptr);
    int getOutputRelayNumber(size_t relayIndex);
    void doRestartProcedure();

    std::vector<std::pair<LogEntry*, uint8_t>> _unsentLogsBuffer;

#ifdef ESP32
    static constexpr uint32_t MIN_FREE_MEMORY = 8 * 1024;
#elif defined(ESP8266)
    static constexpr uint32_t MIN_FREE_MEMORY = 8 * 1024;
#endif

#ifdef ESP8266
    BearSSL::WiFiClientSecure client;
#elif defined(ESP32)
    WiFiClientSecure client;
#endif

#ifdef ESP8266
    Session session;
    X509List certificate;
#endif

    const char* botToken = nullptr;
    bool isBegin = false;
    bool isStop = false;
    bool shouldStartDownload;
    bool isDownloading;
    bool doRestart = false;

    String fileUrl;
    String fileName;
    String document;
    String currentFileName;

    int64_t _lastStatusChatId = 0;
    int32_t _lastStatusMessageId = 0;

    bool isColdStart = true;
    unsigned long lastReconnectAttempt = 0;
    static const unsigned long RECONNECT_INTERVAL = 60000;
    unsigned long startupTime = 0;
};

#endif
