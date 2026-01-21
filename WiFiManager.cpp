#include "WiFiManager.h"

WiFiManager* WiFiManager::instance = nullptr;

const unsigned long WiFiManager::apModeDuration = 10 * 60 * 1000;
const unsigned long WiFiManager::checkInterval = 10000;

WiFiManager::WiFiManager(Settings& ws, TimeModule& tm, Logger& logger, AppState& appState)
  : settings(ws),
    timeModule(tm),
    logger(logger),
    appState(appState),
    apModeStartTime(0),
    lastCheckTime(0),
    wasConnected(false),
    reconnectState(RECONNECT_IDLE),
    reconnectAttempts(0),
    reconnectDelay(1000),
    lastReconnectTime(0),
    checkTime(false) {
    instance = this;
}

void WiFiManager::begin() {

#ifdef ESP8266
  auto eventHandler = [](WiFiEvent_t event) {
    if (WiFiManager::instance) {
      WiFiManager::instance->handleWiFiEvent(event);
    }
  };
  WiFi.onEvent(eventHandler);

#elif defined(ESP32)
  #if ARDUINO_ESP32_MAJOR >= 3
    WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
      if (WiFiManager::instance) {
        WiFiManager::instance->handleWiFiEvent(event);
      }
    });
  #else
    WiFi.onEvent([](arduino_event_id_t event, arduino_event_info_t info) {
      if (WiFiManager::instance) {
        WiFiManager::instance->handleWiFiEvent((WiFiEvent_t)event);
      }
    });
  #endif
#endif

 appState.isStartWifi = true;

  if (settings.ws.isAP) {
    startAPMode();
  } else {
    connectWithFallback();
  }
}

void WiFiManager::loop() {

if (gotIpFlag) {
       settings.ws.urlPath = "http://" + WiFi.localIP().toString();
       logger.addLog("IP: " + settings.ws.urlPath);
       Serial.printf("mDNS: http://%s.local\n", settings.ws.mDNS.c_str());
       startMDNS();

       wasConnected = true;
        reconnectAttempts = 0;
        reconnectDelay = 1000;
        reconnectState = RECONNECT_IDLE;
        settings.ws.isTemporaryAP = false;
        checkTime = true;

       gotIpFlag = false;

}

  if (appState.connectState == AppState::STATE_IDLE) {

  checkConnection();
  handleReconnectStateMachine();

  if (settings.ws.isTemporaryAP) {
    checkTemporaryAP();
  }

if (checkTime && appState.connectState == AppState::CONNECT_IDLE) {
    static bool ntpScheduled = false;
    static unsigned long scheduleTime = 0;

    if (!ntpScheduled) {

        scheduleTime = millis() + 5000;
        ntpScheduled = true;
        Serial.println("NTP update scheduled in 5 seconds...");
    }
    else if (millis() >= scheduleTime) {
        Serial.println("Starting scheduled NTP update...");
        timeModule.updateTime();
        checkTime = false;
        ntpScheduled = false;
    }
}

   #ifdef ESP8266
    static unsigned long lastMDNSUpdate = 0;
    unsigned long currentTime = millis();

  if (currentTime - lastMDNSUpdate >= 5000) {
    lastMDNSUpdate = currentTime;
      MDNS.update();

  }
     #endif
  }
}

void WiFiManager::handleReconnectStateMachine() {
  unsigned long currentTime = millis();

  switch (reconnectState) {
    case RECONNECT_IDLE:

      break;

    case RECONNECT_WAITING:
      if (currentTime - lastReconnectTime >= reconnectDelay) {
        reconnectState = RECONNECT_ATTEMPTING;
      }
      break;

    case RECONNECT_ATTEMPTING:
      if (connectWithFallback()) {
        reconnectState = RECONNECT_IDLE;
        reconnectAttempts = 0;
        reconnectDelay = 1000;
      } else {
        reconnectAttempts++;
        if (reconnectAttempts > 5) {
          Serial.println("Too many reconnect attempts, starting temporary AP for 10 minutes");
          startTemporaryAP();
          reconnectState = RECONNECT_IDLE;
        } else if (!settings.ws.isTemporaryAP) {

          reconnectDelay *= 2;
          if (reconnectDelay > 60000) {
            reconnectDelay = 60000;
          }
          lastReconnectTime = currentTime;
          reconnectState = RECONNECT_WAITING;
          Serial.printf("Reconnect attempt %d failed, next attempt in %dms\n",
                       reconnectAttempts, reconnectDelay);
        }
      }
      break;
  }
}

#ifdef ESP8266
void WiFiManager::onWiFiEvent(WiFiEvent_t event) {
  if (instance) {
    instance->handleWiFiEvent(event);
  }
}
#endif

void WiFiManager::handleWiFiEvent(WiFiEvent_t event) {
  static bool handlingEvent = false;
  if (handlingEvent) {
    return;
  }

  handlingEvent = true;

  switch (event) {

#ifdef ESP8266
    case WIFI_EVENT_STAMODE_CONNECTED:
      Serial.println("Connected to AP");
      break;

    case WIFI_EVENT_STAMODE_GOT_IP:

      gotIpFlag = true;
      appState.isStartWifi = false;
      break;

    case WIFI_EVENT_STAMODE_DISCONNECTED:
      Serial.println("WiFi disconnected");
      wasConnected = false;
       appState.isStartWifi = false;

      if (!settings.ws.isAP && !settings.ws.isTemporaryAP && reconnectState == RECONNECT_IDLE) {
       logger.addLog("Starting reconnect state machine from event handler.");
        reconnectState = RECONNECT_WAITING;
        lastReconnectTime = millis();
      }
      checkTime = false;
      break;

#elif defined(ESP32)

    case ARDUINO_EVENT_WIFI_STA_GOT_IP:

      gotIpFlag = true;
       appState.isStartWifi = false;
      break;

    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.println("WiFi disconnected");
      wasConnected = false;
      appState.isStartWifi = false;

      if (!settings.ws.isAP && !settings.ws.isTemporaryAP && reconnectState == RECONNECT_IDLE) {
        Serial.println("Starting reconnect state machine from event handler.");
        reconnectState = RECONNECT_WAITING;
        lastReconnectTime = millis();
      }
      checkTime = false;
      break;

#endif

  }
  handlingEvent = false;
}

void WiFiManager::checkConnection() {
  unsigned long currentTime = millis();

  if (currentTime - lastCheckTime < checkInterval) return;
  lastCheckTime = currentTime;

  if (settings.ws.isTemporaryAP || settings.ws.isAP || reconnectState != RECONNECT_IDLE) {
    return;
  }

  wl_status_t status = WiFi.status();

  if (status != WL_CONNECTED && wasConnected) {
    Serial.println("Connection check failed, starting reconnect state machine...");
    reconnectState = RECONNECT_WAITING;
    lastReconnectTime = millis();
  }
}

void WiFiManager::startTemporaryAP() {
   appState.isStartWifi = true;
   logger.addLog("Starting temporary AP mode for 10 minutes");
  settings.ws.isTemporaryAP = true;
  apModeStartTime = millis();
  reconnectAttempts = 0;
  reconnectDelay = 1000;
  reconnectState = RECONNECT_IDLE;

#ifdef ESP32
  WiFi.mode(WIFI_MODE_AP);
#elif defined(ESP8266)
  WiFi.mode(WIFI_AP);
#endif

  startAPMode();
  Serial.print("Temporary AP started. IP: ");
  Serial.println(WiFi.softAPIP());
  appState.isStartWifi = false;
}

void WiFiManager::checkTemporaryAP() {
  if (!settings.ws.isTemporaryAP) return;

  unsigned long currentTime = millis();
  unsigned long elapsedTime = currentTime - apModeStartTime;

  if (elapsedTime >= apModeDuration) {
     logger.addLog("Temporary AP mode expired, attempting to reconnect");
    settings.ws.isTemporaryAP = false;

#ifdef ESP32
    WiFi.mode(WIFI_MODE_STA);
#elif defined(ESP8266)
    WiFi.mode(WIFI_STA);
#endif

    reconnectState = RECONNECT_WAITING;
    lastReconnectTime = millis();
  } else {

    static unsigned long lastMinuteCheck = 0;
    unsigned long remainingTime = (apModeDuration - elapsedTime) / 1000;
    if (currentTime - lastMinuteCheck >= 60000) {
      lastMinuteCheck = currentTime;
      Serial.printf("Temporary AP mode: %lu minutes remaining\n", remainingTime / 60);
    }
  }
}

bool WiFiManager::connectWithFallback() {

  appState.isStartWifi = true;

    if (settings.ws.isTemporaryAP) {
        Serial.println("Already in temporary AP mode, skipping connection");
        return false;
    }

    bool shouldStartAP = false;
    String reason = "";

    if (settings.ws.isAP) {
        shouldStartAP = true;
        reason = "AP mode explicitly enabled in settings";
    }

    else if (settings.ws.networkSettings.empty()) {
        shouldStartAP = true;
        reason = "No WiFi networks configured in settings";
    }

    else if (settings.ws.currentIdNetworkSetting < 0 ||
             settings.ws.currentIdNetworkSetting >= (int)settings.ws.networkSettings.size()) {
        shouldStartAP = true;
        reason = "Invalid network index: " + String(settings.ws.currentIdNetworkSetting);
    }

    else {
        const auto& currentNet = settings.ws.networkSettings[settings.ws.currentIdNetworkSetting];

        if (currentNet.ssid.isEmpty() || currentNet.ssid.length() == 0) {
            shouldStartAP = true;
            reason = "Current network has empty SSID";
        }
    }

    if (shouldStartAP) {
        logger.addLog(reason);

        if (settings.ws.isAP) {
            startAPMode();
        } else {
            startTemporaryAP();
        }
        return false;
    }

    Serial.printf("Available networks: %d, Current index: %d\n",
                  settings.ws.networkSettings.size(),
                  settings.ws.currentIdNetworkSetting);

    const auto& primaryNet = settings.ws.networkSettings[settings.ws.currentIdNetworkSetting];
    logger.addLog("Attempting to connect to primary network: " + primaryNet.ssid);

    if (connectToWiFi(primaryNet.ssid, primaryNet.bssid, primaryNet.password)) {
       yield();
       delay(1);
       appState.isStartWifi = false;
        return true;
    }

    if (settings.ws.autoReconnect) {
        logger.addLog("Primary network failed, trying fallback networks...");

        for (size_t i = 0; i < settings.ws.networkSettings.size(); i++) {
            if (i == (size_t)settings.ws.currentIdNetworkSetting) {
                continue;
            }

            const auto& net = settings.ws.networkSettings[i];
            logger.addLog("Attempting fallback to network #" + String(i) + ": " + net.ssid);

            if (connectToWiFi(net.ssid, net.bssid, net.password)) {
              yield();
              delay(1);

                settings.ws.currentIdNetworkSetting = i;
                logger.addLog("Successfully connected to fallback network: " + net.ssid);
                return true;
            }
        }

        logger.addLog("All fallback networks failed");
    } else {
        logger.addLog("Auto-reconnect disabled, not trying other networks");
    }

    Serial.println("All connection attempts failed, starting temporary AP");
    startTemporaryAP();
    appState.isStartWifi = false;
    return false;
}

bool WiFiManager::connectToWiFi(const String& ssid, const String& bssid, const String& password) {
  appState.isStartWifi = true;
  WiFi.disconnect(true);
  delay(100);

#ifdef ESP32
  WiFi.mode(WIFI_MODE_STA);
#elif defined(ESP8266)
  WiFi.mode(WIFI_STA);
#endif

  for (const auto& net : settings.ws.networkSettings) {
    if (net.ssid == ssid && net.useStaticIP) {
      WiFi.config(net.staticIP, net.staticGateway, net.staticSubnet, net.staticDNS);
      break;
    }
  }

  bool useBSSID = (bssid.length() > 0) && (ssid.indexOf(' ') != -1);

  if (useBSSID) {
    uint8_t mac[6];
    if (parseBSSID(bssid, mac)) {
      Serial.printf("Using BSSID connection: %s\n", bssid.c_str());
      return connectWithBSSIDAndChannelScan(ssid, password, mac);
    } else {
      Serial.println("Invalid BSSID format, falling back to SSID");
    }
  }

  appState.isStartWifi = false;
  return connectWithSSID(ssid, password);
}

bool WiFiManager::connectWithBSSIDAndChannelScan(const String& ssid, const String& password, uint8_t* bssid) {

    int configuredChannel = 0;
    for (const auto& net : settings.ws.networkSettings) {
        if (net.ssid == ssid) {
            configuredChannel = net.channel;
            break;
        }
    }

    if (configuredChannel > 0) {
        Serial.printf("Trying fixed channel %d...\n", configuredChannel);

#ifdef ESP8266
        WiFi.begin(ssid.c_str(), password.c_str(), configuredChannel, bssid);
#elif defined(ESP32)
        WiFi.begin(ssid.c_str(), password.c_str(), configuredChannel, bssid);
#endif

        unsigned long startTime = millis();
        while (millis() - startTime < 5000) {
            delay(10);
            if (WiFi.status() == WL_CONNECTED) {
                Serial.printf("Connected on channel %d\n", configuredChannel);
                Serial.printf("IP: %s, RSSI: %d dBm\n",
                            WiFi.localIP().toString().c_str(), WiFi.RSSI());
                return true;
            }
        }
        Serial.println("Fixed channel failed, falling back to scan...");
    }

    const int channels[] = {1, 6, 11, 3, 9, 2, 4, 5, 7, 8, 10, 12, 13};
    const int numChannels = sizeof(channels) / sizeof(channels[0]);

    Serial.println("Scanning channels for BSSID connection...");

    for (int i = 0; i < numChannels; i++) {

        if (channels[i] == configuredChannel) continue;

        Serial.printf("Trying channel %d... ", channels[i]);
        WiFi.disconnect(false);
        delay(50);

#ifdef ESP8266
        WiFi.begin(ssid.c_str(), password.c_str(), channels[i], bssid);
#elif defined(ESP32)
        WiFi.begin(ssid.c_str(), password.c_str(), channels[i], bssid);
#endif

        unsigned long channelStartTime = millis();
        bool connected = false;

        while (millis() - channelStartTime < 3000) {
            delay(10);
            if (WiFi.status() == WL_CONNECTED) {
                connected = true;
                break;
            }
        }

        if (connected) {
            Serial.printf("Success! (Channel %d)\n", channels[i]);
            Serial.printf("IP: %s, RSSI: %d dBm\n",
                        WiFi.localIP().toString().c_str(), WiFi.RSSI());
            return true;
        }

        Serial.println("Failed");
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Connected during channel scan fallback!");
        appState.isStartWifi = false;
        return true;
    }

    Serial.println("All channels failed for BSSID connection");
    appState.isStartWifi = false;
    return false;
}

bool WiFiManager::connectWithSSID(const String& ssid, const String& password) {
  appState.isStartWifi = true;

    int configuredChannel = 0;
    for (const auto& net : settings.ws.networkSettings) {
        if (net.ssid == ssid) {
            configuredChannel = net.channel;
            break;
        }
    }

    logger.addLog("Connecting via SSID: " + ssid);

    if (configuredChannel > 0) {
        Serial.printf("Trying fixed channel %d...\n", configuredChannel);
#ifdef ESP8266
        WiFi.begin(ssid.c_str(), password.c_str(), configuredChannel);
#elif defined(ESP32)
        WiFi.begin(ssid.c_str(), password.c_str(), configuredChannel);
#endif
    } else {
#ifdef ESP8266
        String preparedSSID = ssid;
        if (ssid.indexOf(' ') != -1) {
            preparedSSID = "\"" + ssid + "\"";
            Serial.printf("Using escaped SSID: %s\n", preparedSSID.c_str());
        }
        WiFi.begin(preparedSSID.c_str(), password.c_str());
#else
        WiFi.begin(ssid.c_str(), password.c_str());
#endif
    }

    unsigned long startTime = millis();
    while (millis() - startTime < 8000) {

         yield();
         delay(10);
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\nConnected! IP: " + WiFi.localIP().toString());
            Serial.printf("RSSI: %d dBm\n", WiFi.RSSI());
            return true;
        }

        static unsigned long lastDotTime = 0;
        if (millis() - lastDotTime > 500) {
            Serial.print(".");
            lastDotTime = millis();
        }
    }

    Serial.println("\nConnection failed!");
    appState.isStartWifi = false;
    return false;
}

bool WiFiManager::parseBSSID(const String &bssidStr, uint8_t *bssid) {

  if (bssidStr.length() != 17) return false;

  for (int i = 0; i < 6; i++) {
    int offset = i * 3;
    if (bssidStr[offset + 2] != ':' && i < 5) return false;

    char hex[3] = {bssidStr[offset], bssidStr[offset + 1], '\0'};
    bssid[i] = (uint8_t)strtol(hex, NULL, 16);
  }

  return true;
}

void WiFiManager::startAPMode() {
    appState.isStartWifi = true;
#ifdef ESP32
  WiFi.mode(WIFI_MODE_AP);
#elif defined(ESP8266)
  WiFi.mode(WIFI_AP);
#endif

  if (settings.ws.staticIpAP.toString() != "0.0.0.0") {
    WiFi.softAPConfig(
      settings.ws.staticIpAP,
      settings.ws.staticIpAP,
      IPAddress(255, 255, 255, 0)
    );
  }

WiFi.softAP(settings.ws.ssidAP.c_str(), settings.ws.passwordAP.c_str());

 unsigned long start = millis();
    while (millis() - start < 500) {
        yield();
        delay(1);
    }

IPAddress apIP = WiFi.softAPIP();

String apIPString = apIP.toString();
settings.ws.urlPath = "http://" + apIPString;

startMDNS();

logger.addLog("Запущена точка доступа: "+ settings.ws.ssidAP + " " + settings.ws.urlPath);
appState.isStartWifi = false;

}

bool WiFiManager::isReconnecting() const {

  return reconnectState != RECONNECT_IDLE;
}

void WiFiManager::processScanResults(int numNetworks) {
  if (numNetworks <= 0) {
    scannedNetworks = "[]";
    isScanning = false;
    return;
  }

  int estimatedSize = numNetworks * 100 + 10;
  scannedNetworks = "";
  scannedNetworks.reserve(estimatedSize);
  scannedNetworks = "[";

  for (int i = 0; i < numNetworks; ++i) {
    if (i > 0) scannedNetworks += ",";

    scannedNetworks += "{\"ssid\":\"";
    scannedNetworks += WiFi.SSID(i);
    scannedNetworks += "\",\"rssi\":";
    scannedNetworks += String(WiFi.RSSI(i));
    scannedNetworks += ",\"channel\":";
    scannedNetworks += String(WiFi.channel(i));
    scannedNetworks += ",\"encryption\":\"";
    scannedNetworks += getEncryptionType(i);
    scannedNetworks += "\",\"bssid\":\"";
    scannedNetworks += WiFi.BSSIDstr(i);
    scannedNetworks += "\"}";
  }

  scannedNetworks += "]";
  WiFi.scanDelete();
  isScanning = false;
}

void WiFiManager::scanNetworks() {
  if (isScanning) return;
  isScanning = true;
  scannedNetworks = "[]";

  WiFi.mode(WIFI_AP_STA);
  delay(100);
  WiFi.scanNetworks(true, true);
}

void WiFiManager::startMDNS() {
  if (!MDNS.begin(settings.ws.mDNS.c_str())) {
    Serial.println("Error starting mDNS responder!");
    return;
  }

  Serial.printf("mDNS started: http://%s.local\n", settings.ws.mDNS.c_str());

  MDNS.addService("http", "tcp", 80);

  Serial.println("mDNS responder started successfully");
}

String WiFiManager::getEncryptionType(uint8_t i) {
#ifdef ESP32
  wifi_auth_mode_t encryptionType = WiFi.encryptionType(i);
  switch (encryptionType) {
    case WIFI_AUTH_OPEN: return "Open";
    case WIFI_AUTH_WEP: return "WEP";
    case WIFI_AUTH_WPA_PSK: return "WPA-PSK";
    case WIFI_AUTH_WPA2_PSK: return "WPA2-PSK";
    case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/WPA2-PSK";
    case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-Enterprise";
    case WIFI_AUTH_WPA3_PSK: return "WPA3-PSK";
    case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA2/WPA3-PSK";
    default: return "Unknown";
  }
#elif defined(ESP8266)
  uint8_t encryptionType = WiFi.encryptionType(i);
  switch (encryptionType) {
    case ENC_TYPE_NONE: return "Open";
    case ENC_TYPE_WEP: return "WEP";
    case ENC_TYPE_TKIP: return "WPA-PSK";
    case ENC_TYPE_CCMP: return "WPA2-PSK";
    case ENC_TYPE_AUTO: return "WPA/WPA2-PSK";
    default: return "Unknown";
  }
#endif
}
