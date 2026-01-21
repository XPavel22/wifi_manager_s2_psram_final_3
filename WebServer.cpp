#include "WebServer.h"

WebServer* WebServer::instance = nullptr;

WebServer::WebServer(WiFiManager& wifiManager, Settings& ws, DeviceManager& deviceManager,  TimeModule& timeModule, Info& sysInfo, Ota& ota, Logger& logger, AppState& appState)
  : wifiManager(wifiManager),
    appState(appState),
    settings(ws),
    deviceManager(deviceManager),
    timeModule(timeModule),
    sysInfo(sysInfo),
    ota(ota),
    logger(logger),
    isControlOpen(false)
{
  instance = this;

  logger.setNewLogCallback([this](const LogEntry & entry) {
    this->broadcastNewLog(entry);
  });
}

WebServer::~WebServer() {
  logger.removeNewLogCallback();

  if (saveJson.rawPayload != nullptr) {
    free(saveJson.rawPayload);
    saveJson.rawPayload = nullptr;
    Serial.println("[WebServer] Destructor: Cleaned up pending saveJson buffer.");
  }
}

void WebServer::begin() {

  webSocket.begin();

webSocket.onEvent([](uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
if (WebServer::instance) {

WebServer::instance->handleWebSocketEvent(num, type, payload, length);

}
});

  DefaultHeaders::Instance().addHeader("Connection", "close");

  server.on("/", HTTP_GET, [this](AsyncWebServerRequest * request) {
_webServerIsBusy = true;
    request->send(getIndexResponse(request));
    _webServerIsBusy = false;
  });

  server.onNotFound([this](AsyncWebServerRequest * request) {
    request->send(getIndexResponse(request));
  });

  server.on("/uploadFile", HTTP_POST,
  [](AsyncWebServerRequest * request) {
    request->send(200);
  },
  [this](AsyncWebServerRequest * request, String filename, size_t index,
         uint8_t *data, size_t len, bool final) {

         if (index == 0) {
        _webServerIsBusy = true;
        Serial.println("[WebServer] File upload started. Server is now busy.");
      }
    this->ota.handleFileUpload(request, filename, index, data, len, final);

     if (final) {
        _webServerIsBusy = false;
        Serial.println("[WebServer] File upload finished. Server is now free.");
      }
  }
           );

  server.on("/download", HTTP_POST, [](AsyncWebServerRequest * request) {
    if (request->hasParam("file", true)) {
      String filename = request->getParam("file", true)->value();
      filename = "/" + filename;

#ifdef ESP32
      if (!SPIFFS.begin(true)) {
#elif defined(ESP8266)
      if (!SPIFFS.begin()) {
#endif
        Serial.println("Failed to mount SPIFFS");
      }

      if (!SPIFFS.exists(filename)) {
        request->send(404, "text/plain", "Файл не найден");
        return;
      }

      request->send(SPIFFS, filename, "application/octet-stream");
    } else {
      request->send(400, "text/plain", "Некорректный запрос");
    }
  });

  server.begin();
  Serial.println("WEB SERVER IS BEGIN");
}

void WebServer::startServer() {
  begin();
}

void WebServer::stopServer() {
  server.end();
  Serial.println("WEB SERVER IS STOP");
}

AsyncWebServerResponse * WebServer::getIndexResponse(AsyncWebServerRequest * request) {

  bool hasHtml = SPIFFS.exists("/index.html");
  bool hasGz = SPIFFS.exists("/index.html.gz");
  time_t htmlTime = 0, gzTime = 0;

  time_t now = time(nullptr);

  Serial.printf("[WebServer] File check: HTML=%s, GZ=%s\n",
                hasHtml ? "yes" : "no", hasGz ? "yes" : "no");

  if (hasHtml) {
    File htmlFile = SPIFFS.open("/index.html", "r");
    if (htmlFile) {
      htmlTime = htmlFile.getLastWrite();
      htmlFile.close();
      Serial.printf("[WebServer] HTML file time: %s", ctime(&htmlTime));
    }
  }

  if (hasGz) {
    File gzFile = SPIFFS.open("/index.html.gz", "r");
    if (gzFile) {
      gzTime = gzFile.getLastWrite();
      gzFile.close();
      Serial.printf("[WebServer] GZ file time: %s", ctime(&gzTime));
    }
  }

  AsyncWebServerResponse *response = nullptr;
  String selectedFile = "";
  String reason = "";

  if (hasHtml && (!hasGz || htmlTime > gzTime)) {
    response = request->beginResponse(SPIFFS, "/index.html", "text/html");
    selectedFile = "/index.html";
    if (!hasGz) {
      reason = "GZ file not exists";
    } else {
      reason = "HTML is newer (HTML: " + String(htmlTime) + " > GZ: " + String(gzTime) + ")";
    }
  }
  else if (hasGz) {
    response = request->beginResponse(SPIFFS, "/index.html.gz", "text/html");
    response->addHeader("Content-Encoding", "gzip");
    selectedFile = "/index.html.gz";
    if (!hasHtml) {
      reason = "HTML file not exists";
    } else {
      reason = "GZ is newer or equal (GZ: " + String(gzTime) + " >= HTML: " + String(htmlTime) + ")";
    }
  }
  else {
    response = request->beginResponse_P(200, "text/html", index_html_gz, index_html_gz_len);
    response->addHeader("Content-Encoding", "gzip");
    selectedFile = "EMBEDDED";
    reason = "No files in SPIFFS, using built-in";
  }

  Serial.printf("[WebServer] Selected: %s, Reason: %s\n",  selectedFile.c_str(), reason.c_str());

  response->addHeader("Content-Type", "text/html; charset=utf-8");
  response->addHeader("ETag", "\"" + String(now) + "\"");
  response->addHeader("Date", getHTTPDate(now));

  response->addHeader("Cache-Control", "no-store");
  response->addHeader("Access-Control-Allow-Origin", "*");

  Serial.printf("Free heap after call WEB PAGE: %d\n", ESP.getFreeHeap());

  return response;
}

String WebServer::getHTTPDate(time_t timestamp) {
  struct tm *timeinfo;
  timeinfo = gmtime(&timestamp);
  char buffer[30];
  strftime(buffer, sizeof(buffer), "%a, %d %b %Y %H:%M:%S GMT", timeinfo);
  return String(buffer);
}

void WebServer::broadcastNewLog(const LogEntry& entry) {

  if (loggingClients.empty()) {
    return;
  }

  DynamicJsonDocument doc(512);
  doc["event"] = "new_log";
  doc["timestamp"] = entry.timestamp;
  doc["message"] = entry.message;
  doc["isSay"] = entry.isSay;

  String json;
  serializeJson(doc, json);

  for (uint8_t clientId : loggingClients) {
    webSocket.sendTXT(clientId, json);
  }
  yield();
}

void WebServer::handleSaveSettingsWS(uint8_t num, JsonObject doc) {

  settings.ws.isAP = doc["isAP"] | true;
  settings.ws.ssidAP = doc["ssidAP"] | "ESP-AP";
  settings.ws.mDNS = doc["mDNS"] | "sd";
  settings.ws.passwordAP = doc["passwordAP"] | "";
  settings.ws.autoReconnect = doc["autoReconnect"] | true;

  settings.ws.saveLogs =  doc["saveLogs"] | true;

  logger.setLoggingEnabled(settings.ws.saveLogs);

  if (doc.containsKey("staticIpAP")) {
    settings.ws.staticIpAP.fromString(doc["staticIpAP"] | "192.168.1.1");
  }

  if (doc.containsKey("currentNetworkSsid")) {
    String currentSsid = doc["currentNetworkSsid"] | "";
    settings.ws.currentIdNetworkSetting = -1;

    if (!currentSsid.isEmpty()) {

      for (size_t i = 0; i < settings.ws.networkSettings.size(); i++) {
        if (settings.ws.networkSettings[i].ssid == currentSsid) {
          settings.ws.currentIdNetworkSetting = i;
          break;
        }
      }
    }
  }
  saveNetwork.idClient = num;
  saveNetwork.isSaveNetwork = true;
}

void WebServer::handleSaveNetworkWS(uint8_t num, JsonObject doc) {

  const char* ssid = doc["ssid"] | "";
  if (strlen(ssid) == 0) {
    webSocket.sendTXT(num, "{\"event\":\"error\",\"message\":\"ssid_required\"}");
    return;
  }

  NetworkSetting net;
  net.ssid = doc["ssid"] | "";
  net.password = doc["password"] | "";
  net.useStaticIP = doc["useStaticIP"] | false;
  net.useProxy = doc["useProxy"] | false;
  net.proxy = doc["proxy"] | "";

  if (net.useStaticIP) {
    if (doc.containsKey("ip")) {
      net.staticIP.fromString(doc["ip"] | "0.0.0.0");
    }
    if (doc.containsKey("gateway")) {
      net.staticGateway.fromString(doc["gateway"] | "0.0.0.0");
    }
    if (doc.containsKey("subnet")) {
      net.staticSubnet.fromString(doc["subnet"] | "0.0.0.0");
    }
    if (doc.containsKey("dns")) {
      net.staticDNS.fromString(doc["dns"] | "0.0.0.0");
    }
  }

  bool found = false;
  for (auto& savedNet : settings.ws.networkSettings) {
    if (savedNet.ssid == net.ssid) {
      savedNet = net;
      found = true;
      break;
    }
  }

  if (!found) {
    settings.ws.networkSettings.push_back(net);
  }

  saveNetwork.idClient = num;
  saveNetwork.isSaveNetwork = true;
  saveNetwork.isSendNetworkList = true;
}

void WebServer::handleDeleteNetworkWS(uint8_t num, JsonObject doc) {
  const char* ssid = doc["ssid"] | "";
  int deletedIndex = -1;

  if (strlen(ssid) == 0) {
    webSocket.sendTXT(num, "{\"event\":\"error\",\"message\":\"missing_ssid\"}");
    return;
  }

  for (auto it = settings.ws.networkSettings.begin(); it != settings.ws.networkSettings.end(); ++it) {
    if (it->ssid == ssid) {
      deletedIndex = it - settings.ws.networkSettings.begin();
      settings.ws.networkSettings.erase(it);
      break;
    }
  }

  if (deletedIndex == -1) {
    webSocket.sendTXT(num, "{\"event\":\"error\",\"message\":\"network_not_found\"}");
    return;
  }

  if (settings.ws.currentIdNetworkSetting == deletedIndex) {
    if (settings.ws.networkSettings.empty()) {
      settings.ws.currentIdNetworkSetting = -1;
    } else {
      settings.ws.currentIdNetworkSetting = 0;
    }
  }
  else if (settings.ws.currentIdNetworkSetting > deletedIndex) {
    settings.ws.currentIdNetworkSetting--;
  }

  saveNetwork.idClient = num;
  saveNetwork.isSaveNetwork = true;
  saveNetwork.isSendNetworkList = true;
}

void  WebServer::sendCurrentState(uint8_t num) {
  StaticJsonDocument<2048> doc;

  doc["event"] = "state";
  doc["currentNetwork"] = settings.ws.currentIdNetworkSetting;
  doc["isAP"] = settings.ws.isAP;
  doc["isTemporaryAP"] = settings.ws.isTemporaryAP;
  doc["mDNS"] = settings.ws.mDNS;
  doc["ssidAP"] = settings.ws.ssidAP;
  doc["passwordAP"] = settings.ws.passwordAP;
  doc["staticIpAP"] = settings.ws.staticIpAP.toString();
  doc["urlPath"] = settings.ws.urlPath;
  doc["autoReconnect"] = settings.ws.autoReconnect;
  doc["currentDateTime"] = timeModule.getFormattedDateTime();
  doc["sysInfo"] = sysInfo.getSystemStatus();
  doc["timeZone"] = settings.ws.timeZone;
  doc["saveLogs"] = settings.ws.saveLogs;

  char buffer[2048];
  size_t len = serializeJson(doc, buffer, sizeof(buffer));
  webSocket.sendTXT(num, buffer, len);
}

void WebServer::sendNetworkList(uint8_t num) {
  StaticJsonDocument<1600> doc;
  doc["event"] = "networks";

  doc["isAP"] = settings.ws.isAP;

  JsonArray networks = doc["networks"].to<JsonArray>();

  for (size_t i = 0; i < settings.ws.networkSettings.size(); i++) {
    const auto& net = settings.ws.networkSettings[i];
    JsonObject netObj = networks.createNestedObject();
    netObj["ssid"] = net.ssid;
    netObj["bssid"] = net.bssid;
    netObj["channel"] = net.channel;
    netObj["password"] = net.password;
    netObj["useStaticIP"] = net.useStaticIP;
    netObj["useProxy"] = net.useProxy;
    netObj["proxy"] = net.proxy;
    netObj["isCurrent"] = (i == settings.ws.currentIdNetworkSetting);

    if (net.useStaticIP) {
      netObj["staticIP"] = net.staticIP.toString();
      netObj["staticGateway"] = net.staticGateway.toString();
      netObj["staticSubnet"] = net.staticSubnet.toString();
      netObj["staticDNS"] = net.staticDNS.toString();
    }

    if (i % 10 == 0) {
            yield();
            delay(0);
        }
  }

  char buffer[1600];
  size_t len = serializeJson(doc, buffer, sizeof(buffer));
  webSocket.sendTXT(num, buffer, len);
}

void WebServer::sendSettingsTelegram(uint8_t num) {

  StaticJsonDocument<1024> doc;

  doc["event"] = "telegram";

  doc["isTelegramOn"] = settings.ws.telegramSettings.isTelegramOn;
  doc["botId"] = settings.ws.telegramSettings.botId;

  JsonArray isPushArray = doc.createNestedArray("isPush");
  for (bool pushSetting : settings.ws.telegramSettings.isPush) {
    isPushArray.add(pushSetting);
  }

  JsonArray users = doc.createNestedArray("telegramUsers");
  for (const auto& user : settings.ws.telegramSettings.telegramUsers) {
    JsonObject userObj = users.createNestedObject();
    userObj["id"] = user.id;
    userObj["reading"] = user.reading;
    userObj["writing"] = user.writing;
  }

  char buffer[1024];
  size_t len = serializeJson(doc, buffer, sizeof(buffer));
  webSocket.sendTXT(num, buffer, len);
}

void WebServer::sendSettingsDevice(uint8_t num) {
  const Device& currentDevice = deviceManager.myDevices[deviceManager.currentDeviceIndex];

  StaticJsonDocument<4096> doc;

  doc["event"] = "device_setting";

  String deviceJson = deviceManager.serializeDevice(currentDevice);
  DynamicJsonDocument deviceDoc(4096);
  DeserializationError error = deserializeJson(deviceDoc, deviceJson);

  if (error) {
    Serial.printf("[WebServer] ERROR: Failed to parse device JSON for sending to client. Error: %s\n", error.c_str());

    webSocket.sendTXT(num, "{\"event\":\"device_error\",\"message\":\"Failed to serialize device data\"}");
    return;
  }

  JsonObject deviceObj = deviceDoc.as<JsonObject>();
  for (JsonPair kv : deviceObj) {
    doc[kv.key()] = kv.value();
  }

  String output;
  serializeJson(doc, output);
  webSocket.sendTXT(num, output);
}

void WebServer::handleSaveSettingsDevice(uint8_t num, JsonObject json) {
  bool success = false;

  if (json.containsKey("deviceSettings")) {
    Serial.println("DeviceSettings: call seve");

    JsonObject deviceSettings = json["deviceSettings"];
    Device& currentDevice = deviceManager.myDevices[deviceManager.currentDeviceIndex];
    success = deviceManager.deserializeDevice(deviceSettings, currentDevice);

    deviceManager.isSaveControl = true;
  } else {
    Serial.println("ERROR: Key 'deviceSettings' not found in JSON");
    success = false;
  }

  if (success) {
    webSocket.sendTXT(num, "{\"event\":\"device\",\"success\":true,\"message\":\"DeviceManager settings saved successfully\"}");
    deviceManager.isSaveControl = true;
    Serial.println("DeviceManager settings saved successfully");
  } else {
    webSocket.sendTXT(num, "{\"event\":\"device\",\"success\":false,\"message\":\"Failed to save DeviceManager settings\"}");
    Serial.println("Failed to save DeviceManager settings");
  }
}

void WebServer::handleSaveTelegramSettingsWS(uint8_t num, JsonObject json) {

  settings.ws.telegramSettings.isTelegramOn = json["isTelegramOn"] | false;
  settings.ws.telegramSettings.botId = json["botId"] | "";

  settings.ws.telegramSettings.isPush[0] = false;
  settings.ws.telegramSettings.isPush[1] = false;
  settings.ws.telegramSettings.isPush[2] = false;

  if (json.containsKey("isPush")) {
    JsonArray isPushJsonArray = json["isPush"];

    for (int i = 0; i < 3 && i < isPushJsonArray.size(); i++) {
      settings.ws.telegramSettings.isPush[i] = isPushJsonArray[i];
    }
  }

  settings.ws.telegramSettings.telegramUsers.clear();

  JsonArray users = json["telegramUsers"].as<JsonArray>();
  for (JsonObject user : users) {
    TelegramUser telegramUser;
    telegramUser.id = user["id"] | "";
    telegramUser.reading = user["reading"] | false;
    telegramUser.writing = user["writing"] | false;

    if (!telegramUser.id.isEmpty()) {
      settings.ws.telegramSettings.telegramUsers.push_back(telegramUser);
    }
  }

  saveNetwork.idClient = num;
  saveNetwork.isSaveNetwork = true;
}

void WebServer::handleSaveDateTime(uint8_t num, JsonObject json) {

  String dateInput = json["date"];
  String timeInput = json["time"];

   int8_t timeZoneValue = 3;
   timeZoneValue = String(json["timeZone"]).toInt();

    if (timeZoneValue < -12 || timeZoneValue > 14) {
        timeZoneValue = 3;
    }

    settings.ws.timeZone = timeZoneValue;

  if (!dateInput.isEmpty() && !timeInput.isEmpty()) {

    bool success = timeModule.setTimeManually(dateInput, timeInput);

    String msg = "{\"event\":\"timeStatus\","
                 "\"success\":" + String(success ? "true" : "false") + ","
                 "\"message\":\"" + String(success ? "Time settings saved successfully" : "Time settings saved failed") + "\""
                 "}";
    webSocket.sendTXT(num, msg);
    return;
  }

  webSocket.sendTXT(num,
                    "{\"event\":\"timeStatus\","
                    "\"success\":false,"
                    "\"message\":\"Invalid date or time format\""
                    "}");
}

void WebServer::handleWebSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {

   if (type == WStype_TEXT && WiFi.status() != WL_CONNECTED && !(WiFi.getMode() & WIFI_AP)) {
    Serial.printf("[WebServer] Rejecting request from %d, WiFi not connected and AP is not running.\n", num);
    webSocket.disconnect(num);
    return;
}

  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Disconnected\n", num);

      loggingClients.erase(std::remove(loggingClients.begin(), loggingClients.end(), num), loggingClients.end());
      if (lastResult.clientNum == num ) {
        isClientConnect = false;
      }

      break;

    case WStype_CONNECTED: {
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[%u] Connected from %d.%d.%d.%d\n", num, ip[0], ip[1], ip[2], ip[3]);

        if (lastResult.clientNum == num ) {
          isClientConnect = true;
        }
        break;
      }

   case WStype_TEXT: {

      DynamicJsonDocument doc(8192);
      DeserializationError error = deserializeJson(doc, payload, length);

      if (error) {
        Serial.printf("[%u] JSON parse error: %s\n", num, error.c_str());

        webSocket.sendTXT(num, "{\"event\":\"error\",\"message\":\"invalid_json\"}");
        return;
      }

      String event = doc["event"] | "";

      if (event == "ping") {

        StaticJsonDocument<256> doc;
        doc["event"] = "pong";
        doc["dt"] = timeModule.getFormattedDateTime();
        doc["freeMem"] = ESP.getFreeHeap();

        String response;
        serializeJson(doc, response);
        webSocket.sendTXT(num, response);
      }
      else if (event == "scan") {
        _webServerIsBusy = true;
        Serial.println("Handling scan request");
        handleScanRequest(num);
        wifiManager.clientID = num;
      }
      else if (event == "get_networks") {
        _webServerIsBusy = true;
        Serial.println("Handling get_networks request");
        sendNetworkList(num);
      }
      else if (event == "get_current_state") {
        _webServerIsBusy = true;
        Serial.println("Handling get_current_state request");
        sendCurrentState(num);
      }
      else if (event == "get_settings_telegram") {
        _webServerIsBusy = true;
        Serial.println("Handling get_settings_telegram request");
        sendSettingsTelegram(num);
      }
      else if (event == "get_settings_device") {
        _webServerIsBusy = true;
        Serial.println("Handling get_settings_device request");
        sendSettings.isSend = true;
        sendSettings.idClient = num;
      }
      else if (event == "saveTelegramSettings") {
        _webServerIsBusy = true;
        Serial.println("Handling saveTelegramSettings request");

        handleSaveTelegramSettingsWS(num, doc.as<JsonObject>());
      }
      else if (event == "saveDeviceSettings") {
        _webServerIsBusy = true;
        Serial.println("Handling saveDeviceSettings request");
        Serial.printf("Payload size: %d bytes\n", length);

        if (saveJson.isSave) {
          Serial.println("[WS] Error: A previous save operation is still pending. Rejecting new request.");
          webSocket.sendTXT(num, "{\"event\":\"error\",\"message\":\"pending_operation\"}");
          break;
        }

        saveJson.rawPayload = (char*)malloc(length);
        if (!saveJson.rawPayload) {
          Serial.println("[WS] FATAL: Failed to allocate memory for device settings payload!");
          webSocket.sendTXT(num, "{\"event\":\"error\",\"message\":\"memory_allocation_failed\"}");
          break;
        }

        memcpy(saveJson.rawPayload, payload, length);
        saveJson.payloadLength = length;
        saveJson.receivedTime = millis();
        saveJson.idClient = num;
        saveJson.isSave = true;

        Serial.printf("Payload (%d bytes) saved. Will be processed in the main loop.\n", length);
      }
      else if (event == "addNetwork") {
        _webServerIsBusy = true;
        Serial.println("Handling addNetwork request");

        String ssid = doc["ssid"] | "";
        if (ssid.isEmpty() ) {
          Serial.printf("Rejected invalid SSID: '%s'\n", ssid.c_str());
          webSocket.sendTXT(num, "{\"event\":\"error\",\"message\":\"invalid_ssid\"}");
          return;
        }
        handleAddNetwork(num, doc.as<JsonObject>());
      }
      else if (event == "updateNetwork") {
        _webServerIsBusy = true;
        Serial.println("Handling updateNetwork request");

        handleUpdateNetwork(num, doc.as<JsonObject>());
      }
      else if (event == "deleteNetwork") {
        _webServerIsBusy = true;
        Serial.println("Handling deleteNetwork request");

        const char* ssid = doc["ssid"] | "";
        Serial.printf("SSID to delete: %s\n", ssid);
        handleDeleteNetworkWS(num, doc.as<JsonObject>());
      }
      else if (event == "saveSettings") {
        _webServerIsBusy = true;
        Serial.println("Handling saveSettings request");

        handleSaveSettingsWS(num, doc.as<JsonObject>());
      }
      else if (event == "saveDateTime") {
        _webServerIsBusy = true;

        handleSaveDateTime(num, doc.as<JsonObject>());
      }
      else if (event == "test_connection") {
        _webServerIsBusy = true;

        String ssid = doc["ssid"] | "";
        String bssid = doc["bssid"] | "";
        String password = doc["password"] | "";
        int channel = doc["channel"] | 0;

        Serial.printf("Test connection to: %s\n", ssid.c_str());

        if (ssid.length() > 0) {
          tryConnectToWiFi(num, ssid, bssid, password, channel);
          Serial.println("Started new connection test");
        }
      }

      else if (event == "event_logs_open") {
        _webServerIsBusy = true;

        Serial.printf("[%u] Opened logs tab. Sending history.\n", num);

        if (std::find(loggingClients.begin(), loggingClients.end(), num) == loggingClients.end()) {
          loggingClients.push_back(num);
        }

        yield();
        delay(1);

        String response = "{\"event\":\"all_logs\",\"logs\":" + logger.getAllLogsJSON() + "}";
        webSocket.sendTXT(num, response);

      }
      else if (event == "event_logs_close") {
        _webServerIsBusy = true;
        Serial.printf("[%u] Closed logs tab.\n", num);

        loggingClients.erase(std::remove(loggingClients.begin(), loggingClients.end(), num), loggingClients.end());

        webSocket.sendTXT(num, "{\"event\":\"logs_closed\",\"message\":\"Log streaming stopped\"}");
        logger.saveLogsToSPIFFS();
      }
      else if (event == "event_logs_clear") {
        logger.clearLogs();
      }
      else if (event == "event_logs_save") {
        _webServerIsBusy = true;
        logger.saveLogsToSPIFFS();
      }

      else if (event == "tab_control_open") {
        _webServerIsBusy = true;
        isControlOpen = true;
      }
      else if (event == "tab_control_close") {
        _webServerIsBusy = true;
        isControlOpen = false;
      }

      else if (event == "relay") {
        _webServerIsBusy = true;
        Serial.println("Handling relay command request");

        bool success = deviceManager.handleRelayCommand(doc.as<JsonObject>(), num);
        String response = "{\"event\":\"relay_command_status\",\"success\":" + String(success ? "true" : "false") + "}";
        webSocket.sendTXT(num, response);
      }
      else if (event == "reboot") {
        _webServerIsBusy = true;
        Serial.println("Rebooting...");

        webSocket.sendTXT(num, "{\"event\":\"reboot\",\"status\":\"initiated\"}");

        delay(50);

        WiFi.disconnect(true);
        delay(100);
        WiFi.mode(WIFI_OFF);
        delay(100);

        logger.saveLogsToSPIFFS();

        delay(100);
        ESP.restart();
      }
      else if (event == "full_reset") {
        _webServerIsBusy = true;
        Serial.println("full reset...");
        webSocket.sendTXT(num, "{\"event\":\"reboot\",\"status\":\"formatting\"}");
        appState.isFormat = true;
      }

      else {
        Serial.printf("[%u] Unknown event: %s\n", num, event.c_str());
      }
      break;
    }

    default:
      break;
  }
  _webServerIsBusy = false;
}

void WebServer::handleUpdateNetwork(uint8_t num, JsonObject doc) {
  const char* originalSsid = doc["originalSsid"] | "";
  if (strlen(originalSsid) == 0) {
    webSocket.sendTXT(num, "{\"event\":\"error\",\"message\":\"missing_original_ssid\"}");
    return;
  }

  NetworkSetting updatedNet;
  updatedNet.ssid = doc["ssid"] | "";
  updatedNet.bssid = doc["bssid"] | "";
  updatedNet.channel = doc["channel"].as<int>() | 0;
  updatedNet.password = doc["password"] | "";
  updatedNet.useStaticIP = doc["useStaticIP"] | false;
  updatedNet.useProxy = doc["useProxy"] | false;
  updatedNet.proxy = doc["proxy"] | "";

  if (updatedNet.useStaticIP) {
    if (doc.containsKey("ip")) updatedNet.staticIP.fromString(doc["ip"] | "0.0.0.0");
    if (doc.containsKey("gateway")) updatedNet.staticGateway.fromString(doc["gateway"] | "0.0.0.0");
    if (doc.containsKey("subnet")) updatedNet.staticSubnet.fromString(doc["subnet"] | "0.0.0.0");
    if (doc.containsKey("dns")) updatedNet.staticDNS.fromString(doc["dns"] | "0.0.0.0");
  }

    bool found = false;

  for (int i = 0; i < settings.ws.networkSettings.size(); i++) {
    if (settings.ws.networkSettings[i].ssid == originalSsid) {
      settings.ws.networkSettings[i] = updatedNet;
      settings.ws.currentIdNetworkSetting = i;
      found = true;

      char sbuffer[128];
      snprintf(sbuffer, sizeof(sbuffer), "Network '%s' updated at index %d", originalSsid, i);
      logger.addLog(sbuffer);

      break;
    }
  }

  if (!found) {
    webSocket.sendTXT(num, "{\"event\":\"error\",\"message\":\"network_not_found\"}");
    return;
  }

  if (doc.containsKey("isAP")) {
    settings.ws.isAP = doc["isAP"];
  }

  saveNetwork.idClient = num;
  saveNetwork.isSaveNetwork = true;
  saveNetwork.isSendNetworkList = true;
}

void WebServer::handleAddNetwork(uint8_t num, JsonObject doc) {
  NetworkSetting net;
  net.ssid = doc["ssid"] | "";
  net.bssid = doc["bssid"] | "";
  net.channel = doc["channel"].as<int>() | 0;
  Serial.println("Channel: " + String(net.channel));
  net.password = doc["password"] | "";
  net.useStaticIP = doc["useStaticIP"] | false;
  net.useProxy = doc["useProxy"] | false;
  net.proxy = doc["proxy"] | "";

  if (net.useStaticIP) {
    if (doc.containsKey("ip")) net.staticIP.fromString(doc["ip"] | "0.0.0.0");
    if (doc.containsKey("gateway")) net.staticGateway.fromString(doc["gateway"] | "0.0.0.0");
    if (doc.containsKey("subnet")) net.staticSubnet.fromString(doc["subnet"] | "0.0.0.0");
    if (doc.containsKey("dns")) net.staticDNS.fromString(doc["dns"] | "0.0.0.0");
  }

  for (const auto& savedNet : settings.ws.networkSettings) {
    if (savedNet.ssid == net.ssid) {
      webSocket.sendTXT(num, "{\"event\":\"error\",\"message\":\"network_already_exists\"}");
      return;
    }
  }

  settings.ws.networkSettings.push_back(net);

  if (settings.ws.networkSettings.size() == 1) {
    settings.ws.currentIdNetworkSetting = 0;
  }

  saveNetwork.idClient = num;
  saveNetwork.isSaveNetwork = true;
  saveNetwork.isSendNetworkList = true;
}

void WebServer::handleScanRequest(uint8_t num) {
  if (wifiManager.isScanning) {
    webSocket.sendTXT(num, "{\"event\":\"error\",\"message\":\"scan_already_in_progress\"}");
    return;
  }
  wifiManager.scanNetworks();
  webSocket.sendTXT(num, "{\"event\":\"scan_started\"}");
}

unsigned long connectStartTime;
WiFiMode_t connectOriginalMode;
String connectOriginalSSID;
String connectOriginalPass;
int connectTestChannel = 0;
bool connectOriginalAutoReconnect;
bool connectWasConnected;
String connectTestSSID;
String connectTestBSSID;
String connectTestPass;
bool connectResult;
bool connectRequested = false;

bool WebServer::tryConnectToWiFi(uint8_t clientNum,
                                 const String & ssid,
                                 const String & bssid,
                                 const String & password,
                                 int channel) {
  if (appState.connectState == AppState::CONNECT_IDLE && !connectRequested) {
    connectTestSSID = ssid;
    connectTestBSSID = bssid;
    connectTestPass = password;
    connectTestChannel = channel;
    currentClientNum = clientNum;
    connectRequested = true;
    isTryConnect = true;
    return true;
  }
  return false;
}

void WebServer::processWiFiConnection() {
  switch (appState.connectState) {
    case AppState::CONNECT_IDLE:
      if (connectRequested) {
        Serial.println("[WiFiConnect] Starting connection test...");
        connectRequested = false;

        connectOriginalMode = WiFi.getMode();
        connectOriginalSSID = WiFi.SSID();
        connectOriginalPass = WiFi.psk();
        connectOriginalAutoReconnect = WiFi.getAutoReconnect();
        connectWasConnected = (WiFi.status() == WL_CONNECTED);

        appState.connectState = AppState::CONNECT_PREPARING;
      }
      break;

    case AppState::CONNECT_PREPARING:
     _webServerIsBusy = true;
      WiFi.disconnect(false);
      delay(100);

      if (connectOriginalMode != WIFI_AP_STA) {
        WiFi.mode(WIFI_AP_STA);
      }

      WiFi.setAutoReconnect(false);
      appState.connectState = AppState::CONNECT_STARTED;
      connectStartTime = millis();
      break;

    case AppState::CONNECT_STARTED:
      {
        Serial.printf("[WiFiConnect] Connecting to: %s (channel=%d)\n",
                      connectTestSSID.c_str(), connectTestChannel);

        bool useBSSID = (connectTestSSID.indexOf(' ') != -1) &&
                        (connectTestBSSID.length() > 0);

        if (useBSSID) {
          Serial.printf("[WiFiConnect] Using BSSID for connection: %s Channel: %d\n ", connectTestBSSID.c_str(), connectTestChannel);

          uint8_t bssid[6];
          if (parseBSSID(connectTestBSSID, bssid)) {

            WiFi.begin(connectTestSSID.c_str(), connectTestPass.c_str(), 0, bssid);
          }  else {
            WiFi.begin(connectTestSSID.c_str(), connectTestPass.c_str(), 0, bssid);
          }
        } else {
#ifdef ESP8266
          String preparedSSID = connectTestSSID;
          if (connectTestSSID.indexOf(' ') != -1) {
            preparedSSID = "\"" + connectTestSSID + "\"";
          }
          if (connectTestChannel > 0) {
            WiFi.begin(preparedSSID.c_str(), connectTestPass.c_str(),
                       connectTestChannel);
          } else {
            WiFi.begin(preparedSSID.c_str(), connectTestPass.c_str());
          }
#else
          if (connectTestChannel > 0) {
            WiFi.begin(connectTestSSID.c_str(), connectTestPass.c_str(),
                       connectTestChannel);
          } else {
            WiFi.begin(connectTestSSID.c_str(), connectTestPass.c_str());
          }
#endif
        }

        appState.connectState = AppState::CONNECT_WAITING;
        connectStartTime = millis();
        break;
      }

    case AppState::CONNECT_WAITING:
      {
        wl_status_t status = WiFi.status();

        if (status == WL_CONNECTED) {
          Serial.println("[WiFiConnect] Connection successful!");
          Serial.printf("IP: %s, RSSI: %d dBm\n",
                        WiFi.localIP().toString().c_str(), WiFi.RSSI());
          connectResult = true;
          appState.connectState = AppState::CONNECT_COMPLETING;
        } else if (millis() - connectStartTime > 15000) {
          Serial.printf("[WiFiConnect] Connection timeout. Status: %d - ", status);
          printWiFiStatus(status);
          connectResult = false;
          appState.connectState = AppState::CONNECT_COMPLETING;
        } else {

          static unsigned long lastStatusPrint = 0;
          if (millis() - lastStatusPrint > 2000) {
            Serial.printf("[WiFiConnect] Current status: %d - ", status);
            printWiFiStatus(status);
            lastStatusPrint = millis();
          }

          yield();
          delay(10);
        }
        break;
      }

    case AppState::CONNECT_COMPLETING:
      WiFi.disconnect(false);
      delay(100);

      if (WiFi.getMode() != connectOriginalMode) {
        WiFi.mode(connectOriginalMode);
      }

      if (connectWasConnected && connectOriginalSSID.length() > 0) {
        Serial.printf("[WiFiConnect] Restoring connection to: %s\n", connectOriginalSSID.c_str());
        WiFi.begin(connectOriginalSSID.c_str(), connectOriginalPass.c_str());
      }

      WiFi.setAutoReconnect(connectOriginalAutoReconnect);

      lastResult.clientNum = currentClientNum;
      lastResult.success = connectResult;
      lastResult.sent = false;
      lastResult.timestamp = millis();

      Serial.printf("[WiFiConnect] Result saved for client %d: %s\n",
                    lastResult.clientNum,
                    lastResult.success ? "success" : "failed");

      appState.connectState = AppState::CONNECT_IDLE;
      isTryConnect = false;
      _webServerIsBusy = false;
      break;
  }
}

bool WebServer::parseBSSID(const String & bssidStr, uint8_t *bssid) {

  if (bssidStr.length() != 17) return false;

  for (int i = 0; i < 6; i++) {
    int offset = i * 3;
    if (bssidStr[offset + 2] != ':' && i < 5) return false;

    char hex[3] = {bssidStr[offset], bssidStr[offset + 1], '\0'};
    bssid[i] = (uint8_t)strtol(hex, NULL, 16);
  }

  return true;
}

void WebServer::printWiFiStatus(wl_status_t status) {
  switch (status) {
    case WL_NO_SHIELD: Serial.println("WL_NO_SHIELD"); break;
    case WL_IDLE_STATUS: Serial.println("WL_IDLE_STATUS"); break;
    case WL_NO_SSID_AVAIL: Serial.println("WL_NO_SSID_AVAIL"); break;
    case WL_SCAN_COMPLETED: Serial.println("WL_SCAN_COMPLETED"); break;
    case WL_CONNECTED: Serial.println("WL_CONNECTED"); break;
    case WL_CONNECT_FAILED: Serial.println("WL_CONNECT_FAILED"); break;
    case WL_CONNECTION_LOST: Serial.println("WL_CONNECTION_LOST"); break;
    case WL_DISCONNECTED: Serial.println("WL_DISCONNECTED"); break;
    default: Serial.println("UNKNOWN_STATUS"); break;
  }
}

void WebServer::processPendingResultsScan() {
  if (lastResult.sent || lastResult.timestamp == 0) return;

  if (millis() - lastResult.timestamp > 25000) {
    Serial.printf("[%u] Result expired\n", lastResult.clientNum);
    lastResult.sent = true;
    return;
  }

  if (!lastResult.sent && isClientConnect) {
    StaticJsonDocument<200> doc;
    doc["event"] = "test_result";
    doc["ssid"] = connectTestSSID;
    doc["bssid"] = connectTestBSSID;
    doc["channel"] = connectTestChannel;
    doc["success"] = lastResult.success;

    if (lastResult.success) {
      doc["ip"] = WiFi.localIP().toString();
      doc["rssi"] = WiFi.RSSI();
    }

    char buffer[250];
    size_t len = serializeJson(doc, buffer, sizeof(buffer));

    lastResult.sent = webSocket.sendTXT(lastResult.clientNum, buffer, len);
    Serial.printf("[%u] Result: %s\n", lastResult.clientNum,
                  lastResult.success ? "success" : "failed");
    lastResult.sent = true;
  }
}

void WebServer::loop() {
  webSocket.loop();

  if (saveJson.isSave) {

    if (millis() - saveJson.receivedTime > 5000) {
      Serial.println("[WS] Save request timeout");
      webSocket.sendTXT(saveJson.idClient, "{\"event\":\"error\",\"message\":\"timeout\"}");

      free(saveJson.rawPayload);
      saveJson.rawPayload = nullptr;
      saveJson.isSave = false;
    } else {

      DynamicJsonDocument doc(8192);

      Serial.printf("[WS] Deserializing JSON, size=%d, heap=%d\n", saveJson.payloadLength, ESP.getFreeHeap());
      DeserializationError error = deserializeJson(doc, saveJson.rawPayload, saveJson.payloadLength);

      if (error) {
        Serial.printf("[WS] JSON deserialize error: %s\n", error.c_str());
        webSocket.sendTXT(saveJson.idClient, "{\"event\":\"error\",\"message\":\"deserialize_failed\"}");
      } else {
        Serial.println("[WS] JSON deserialized successfully, calling handler.");
        yield();
        handleSaveSettingsDevice(saveJson.idClient, doc.as<JsonObject>());
      }

      free(saveJson.rawPayload);
      saveJson.rawPayload = nullptr;
      saveJson.isSave = false;
      Serial.println("[WS] Memory for saveJson freed.");
    }
  }

  if (saveNetwork.isSaveNetwork) {
    bool result = settings.saveSettings();

    delay(10);
    Serial.println("isSaveNetwork " + String(result));

    if (result) {
      webSocket.sendTXT(saveNetwork.idClient, "{\"event\":\"settingsStatus\",\"success\":true,\"message\":\"Settings saved successfully\"}");

      if (saveNetwork.isSendNetworkList) {
        webSocket.sendTXT(saveNetwork.idClient, "{\"event\":\"network_updated\",\"success\":true}");
        sendNetworkList(saveNetwork.idClient);
        saveNetwork.isSendNetworkList = false;
      }
    } else {
      webSocket.sendTXT(saveNetwork.idClient, "{\"event\":\"settingsStatus\",\"success\":false,\"message\":\"Failed to save settings\"}");
    }

    saveNetwork.isSaveNetwork = false;
  }

  if (sendSettings.isSend) {
    sendSettingsDevice(sendSettings.idClient);
    sendSettings.isSend = false;
  }

  processWiFiConnection();
  processPendingResultsScan();

  if (wifiManager.isScanning) {

    int scanStatus = WiFi.scanComplete();

    if (scanStatus >= 0) {
      Serial.printf("[SCAN] Scan completed with %d networks found\n", scanStatus);
      wifiManager.processScanResults(scanStatus);

      webSocket.broadcastTXT("{\"event\":\"scan_complete\"}");

      Serial.println("[SCAN] Broadcast scan_complete event");

      String json = "{\"event\":\"scan_results\",\"networks\":" + wifiManager.scannedNetworks + "}";
      webSocket.sendTXT(wifiManager.clientID, json);

    } else if (scanStatus == WIFI_SCAN_FAILED) {
      Serial.println("[SCAN] Scan failed (WIFI_SCAN_FAILED)");

      webSocket.broadcastTXT("{\"event\":\"scan_failed\"}");
    }
  }

  if (webSocket.connectedClients() > 0 && isControlOpen) {
    Device& currentDevice = deviceManager.myDevices[deviceManager.currentDeviceIndex];

    if (deviceManager.relayStateChanged()) {
      String output = deviceManager.serializeRelaysForControlTab();
      yield();
      webSocket.broadcastTXT(output);
    }

    if (deviceManager.checkSettingsChanged()) {
      String output = deviceManager.serializeDeviceFlags();
       webSocket.broadcastTXT(output);
    }

   if (deviceManager.timersProgressChanged()) {

    static unsigned long lastTimerUpdate = 0;
    const unsigned long TIMER_UPDATE_INTERVAL = 500;

    if (millis() - lastTimerUpdate >= TIMER_UPDATE_INTERVAL) {
      lastTimerUpdate = millis();

        String output = deviceManager.serializeTimersProgress();
        webSocket.broadcastTXT(output);

    }
  }

     static unsigned long lastBroadcastTime = 0;
     const unsigned long BROADCAST_INTERVAL = 500;

if (deviceManager.sensorValuesChanged() && (millis() - lastBroadcastTime > BROADCAST_INTERVAL)) {
    String output = deviceManager.serializeSensorValues();
    webSocket.broadcastTXT(output);
    yield();

    lastBroadcastTime = millis();
}

  }

  if ( appState.isFormat ) {
    settings.format();
  }

}

bool WebServer::isBusy() const {
    return _webServerIsBusy;
}
