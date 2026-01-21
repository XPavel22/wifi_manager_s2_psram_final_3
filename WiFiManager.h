#pragma once

#ifdef ESP8266
  #include <ESP8266WiFi.h>
#elif defined(ESP32)
  #include <WiFi.h>
#endif

#include <vector>
#include "ConfigSettings.h"
#include "TimeModule.h"
#include "Logger.h"
#include "AppState.h"

#ifdef ESP8266
  #include <ESP8266mDNS.h>
#elif defined(ESP32)
  #include <ESPmDNS.h>
#endif

class WiFiManager {
public:

    enum ReconnectState {
        RECONNECT_IDLE,
        RECONNECT_WAITING,
        RECONNECT_ATTEMPTING
    };

    WiFiManager(Settings& ws, TimeModule& tm, Logger& logger, AppState& appState);

    void begin();
    void loop();

    bool connectToWiFi(const String& ssid, const String& bssid, const String& password);
    bool connectWithSSID(const String& ssid, const String& password);
    bool connectWithBSSIDAndChannelScan(const String& ssid, const String& password, uint8_t* bssid);
    void scanNetworks();
    void processScanResults(int numNetworks);
    bool connectWithFallback();
    void startAPMode();
    bool isReconnecting() const;

#ifdef ESP8266
    static void onWiFiEvent(WiFiEvent_t event);
#elif defined(ESP32)
  #if ARDUINO_ESP32_MAJOR >= 3
    static void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info);
  #else
    static void onWiFiEvent(arduino_event_id_t event, arduino_event_info_t info);
  #endif
#endif

void handleWiFiEvent(WiFiEvent_t event);

    String getEncryptionType(uint8_t i);

    String scannedNetworks;
    bool isScanning = false;
    uint8_t clientID;
    bool checkTime = false;
    bool wasConnected;

    static WiFiManager* instance;

private:

    Settings& settings;
    TimeModule& timeModule;
    Logger& logger;
    AppState& appState;

    void checkConnection();
    void handleReconnectStateMachine();
    void startTemporaryAP();
    void checkTemporaryAP();

    bool parseBSSID(const String &bssidStr, uint8_t *bssid);

    bool gotIpFlag = false;
    void startMDNS();

    unsigned long apModeStartTime;
    unsigned long lastCheckTime;
    unsigned long lastDisconnectTime = 0;

    ReconnectState reconnectState;
    int reconnectAttempts;
    unsigned long reconnectDelay;
    unsigned long lastReconnectTime;

    static const unsigned long apModeDuration;
    static const unsigned long checkInterval;

#ifdef ESP32
    portMUX_TYPE wifiEventMutex = portMUX_INITIALIZER_UNLOCKED;
#endif
};
