#pragma once
#include "CommonTypes.h"
#include "WiFiManager.h"
#include "DeviceManager.h"
#include "ConfigSettings.h"
#include "TimeModule.h"
#include "Info.h"
#include "Logger.h"
#include "Ota.h"
#include "index_html_gz.h"
#include "AppState.h"
#include <ESPAsyncWebServer.h>
#include <WebSocketsServer.h>

class WebServer {

  public:
    AsyncWebServer server{80};
    static WebServer* instance;

    WebServer(WiFiManager& wifiManager, Settings& ws, DeviceManager& deviceManager,  TimeModule& timeModule, Info& sysInfo, Ota& ota, Logger& logger, AppState& appState );
    ~WebServer();

    void begin();
    void loop();
    void startServer();
    void stopServer();
    bool isTryConnect = false;

    struct SaveNetwork {
      bool isSaveNetwork = false;
      bool isSendNetworkList = false;
      uint8_t idClient;
    };

    SaveNetwork saveNetwork;

     bool isBusy() const;

  private:
    Settings& settings;
    WiFiManager& wifiManager;
    DeviceManager& deviceManager;
    TimeModule& timeModule;
    Info& sysInfo;
    Ota& ota;
    Logger& logger;
    AppState& appState;

    WebSocketsServer webSocket{81};

    AsyncWebServerResponse* getIndexResponse(AsyncWebServerRequest *request);
    String getHTTPDate(time_t timestamp);

    void handleWebSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length);

    void handleSaveSettingsWS(uint8_t num, JsonObject doc);
    void handleSaveNetworkWS(uint8_t num, JsonObject doc);
    void handleDeleteNetworkWS(uint8_t num, JsonObject doc);
    void handleSetCurrentNetwork(uint8_t num, JsonObject doc);
    void handleSaveTelegramSettingsWS(uint8_t num, JsonObject json);
    void handleSaveSettingsDevice(uint8_t num, JsonObject json);
    void handleSaveDateTime(uint8_t num, JsonObject json);

    void sendCurrentState(uint8_t num);
    void sendNetworkList(uint8_t num);
    void sendSettingsTelegram(uint8_t num);
    void sendSettingsDevice(uint8_t num);

    void handleScanRequest(uint8_t num);
    void handleAddNetwork(uint8_t num, JsonObject doc);
    void handleUpdateNetwork(uint8_t num, JsonObject doc);

    void printWiFiStatus(wl_status_t status);

std::vector<uint8_t> loggingClients;

void broadcastNewLog(const LogEntry& entry);

    struct SaveJson {
  bool isSave = false;
  char* rawPayload = nullptr;
  size_t payloadLength = 0;
  unsigned long receivedTime;
  uint8_t idClient;

  SaveJson() : rawPayload(nullptr), payloadLength(0), receivedTime(0) {}
};

    SaveJson saveJson;

    struct SendSettings {
      bool isSend = false;
      uint8_t idClient;
    };

    SendSettings sendSettings;

    static void handleFileUploadWrapper(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *fileData, size_t len, bool final) {
      if (instance) {
        instance->ota.handleFileUpload(request, filename, index, fileData, len, final);
      }
    }

    bool tryConnectToWiFi(uint8_t clientNum, const String & ssid, const String & bssid, const String & password, int channel);
    void processWiFiConnection();
    void processPendingResultsScan();
    bool parseBSSID(const String &bssidStr, uint8_t *bssid);

    struct ConnectionResult {
      String ssid;
      uint8_t clientNum;
      uint8_t indexNetwork;
      bool success;
      bool sent = false;
      unsigned long timestamp;
    };

    ConnectionResult lastResult;
    uint8_t currentClientNum;

    bool isControlOpen;
    bool isClientConnect = false;
    bool _webServerIsBusy = false;
};
