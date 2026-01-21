#include "ConfigSettings.h"

Settings::Settings(AppState& appState) :
appState(appState),
ws() {}

void Settings::printFsInfo() {
#ifdef CONFIG_IDF_TARGET_ESP32S2
  Serial.printf("SPIFFS Total: %d bytes, Used: %d bytes\n",
                SPIFFS.totalBytes(), SPIFFS.usedBytes());
#elif defined(ESP8266)
  FSInfo fs_info;
  SPIFFS.info(fs_info);
  Serial.printf("SPIFFS Total: %d bytes, Used: %d bytes\n",
                fs_info.totalBytes, fs_info.usedBytes);
#endif
}

void Settings::begin() {

#ifdef CONFIG_IDF_TARGET_ESP32S2
  spiffsMounted = SPIFFS.begin(true);
#elif defined(ESP8266)
  spiffsMounted = SPIFFS.begin();
#endif

  if (spiffsMounted) {
    Serial.println("\nSPIFFS mounted successfully\n");

    printFsInfo();
  } else {
    Serial.println("\nFailed to mount SPIFFS\n");
  }

}

bool Settings::isFSMounted() {
  if (spiffsMounted) {
    return true;
  }

#ifdef ESP32
  spiffsMounted = SPIFFS.begin(true);
#elif defined(ESP8266)
  spiffsMounted = SPIFFS.begin();
#endif

  if (!spiffsMounted)
  {
    Serial.println("Failed to mount SPIFFS in isFSMounted()");
#ifdef ESP32
    SPIFFS.end();
    spiffsMounted = SPIFFS.begin(true);
#elif defined(ESP8266)
    SPIFFS.end();
    spiffsMounted = SPIFFS.begin();
#endif
    return spiffsMounted;
  }

  return spiffsMounted;
}

bool Settings::freeSpaceFS() {

  if (!spiffsMounted) {
    Serial.println("FS not mounted, cannot check free space.");
    return false;
  }

  size_t totalBytes = 0;
  size_t usedBytes = 0;

#ifdef ESP32
  totalBytes = SPIFFS.totalBytes();
  usedBytes = SPIFFS.usedBytes();
#elif defined(ESP8266)
  FSInfo fs_info;
  if (SPIFFS.info(fs_info)) {
    totalBytes = fs_info.totalBytes;
    usedBytes = fs_info.usedBytes;
  } else {
    Serial.println("Failed to get FS info");
    return false;
  }
#endif

  size_t freeBytes = totalBytes - usedBytes;
  const size_t requiredBytes = 10240;

  Serial.printf("FS Info: Total=%u, Used=%u, Free=%u, Required=%u\n",
               totalBytes, usedBytes, freeBytes, requiredBytes);

  if (freeBytes < requiredBytes) {
    Serial.printf("Not enough space in SPIFFS. Free: %u, Required: %u\n", freeBytes, requiredBytes);
    return false;
  }

  return true;
}

bool Settings::loadDefaults(bool saveToFile) {

ws.isWifiTurnedOn = true;
ws.currentIdNetworkSetting = 0;
ws.ssidAP = "Kolibri-AP";
ws.mDNS = "sd";
ws.passwordAP = "";
ws.staticIpAP = IPAddress(192, 168, 1, 1);
ws.isAP = true;
ws.isTemporaryAP = false;
ws.autoReconnect = true;
ws.timeZone = 3;
ws.saveLogs = false;

ws.networkSettings.clear();
NetworkSetting defaultNetwork;
defaultNetwork.ssid = "HomeWiFi";
defaultNetwork.channel = 0;
defaultNetwork.password = "12345678w";
defaultNetwork.useStaticIP = false;
defaultNetwork.useProxy = false;
defaultNetwork.proxy = "";
ws.networkSettings.push_back(defaultNetwork);

ws.telegramSettings.isTelegramOn = false;
ws.telegramSettings.botId = "";

  ws.telegramSettings.isPush[0] = true;
  ws.telegramSettings.isPush[1] = false;
  ws.telegramSettings.isPush[2] = true;

ws.telegramSettings.telegramUsers.clear();
TelegramUser defaultUser;
defaultUser.id = "123456789";
defaultUser.reading = true;
defaultUser.writing = true;
ws.telegramSettings.telegramUsers.push_back(defaultUser);
ws.telegramSettings.lastMessage = "";

if (saveToFile) {
Serial.println("Saving default settings to file");
return saveSettings();
}

Serial.println("Loaded default settings (not saved to file)");
return true;
}

String Settings::serializeSettings(const WiFiSettings& settings) {
  DynamicJsonDocument doc(4096);

  doc["isWifiTurnedOn"] = settings.isWifiTurnedOn;
  doc["currentIdNetworkSetting"] = settings.currentIdNetworkSetting;
  doc["isAP"] = settings.isAP;
  doc["ssidAP"] = settings.ssidAP;
  doc["mDNS"] = settings.mDNS;
  doc["passwordAP"] = settings.passwordAP;
  doc["staticIpAP"] = settings.staticIpAP.toString();
  doc["autoReconnect"] = settings.autoReconnect;
  doc["timeZone"] = settings.timeZone;
  doc["saveLogs"] = settings.saveLogs;

  JsonObject telegram = doc.createNestedObject("telegramSettings");
  telegram["isTelegramOn"] = settings.telegramSettings.isTelegramOn;
  telegram["botId"] = settings.telegramSettings.botId;
  telegram["lastMessage"] = settings.telegramSettings.lastMessage;

  JsonArray isPushArray = telegram.createNestedArray("isPush");
  for (int i = 0; i < 3; i++) {
    isPushArray.add(settings.telegramSettings.isPush[i]);
  }

  JsonArray usersArray = telegram.createNestedArray("telegramUsers");
  for (const auto& user : settings.telegramSettings.telegramUsers) {
    JsonObject userObj = usersArray.createNestedObject();
    userObj["id"] = user.id;
    userObj["reading"] = user.reading;
    userObj["writing"] = user.writing;
  }

  JsonArray networks = doc.createNestedArray("networkSettings");
  for (const auto& net : settings.networkSettings) {
    JsonObject netObj = networks.createNestedObject();
    netObj["ssid"] = net.ssid;
    netObj["bssid"] = net.bssid;
    netObj["channel"] = net.channel;
    netObj["password"] = net.password;
    netObj["useStaticIP"] = net.useStaticIP;
    netObj["useProxy"] = net.useProxy;
    netObj["proxy"] = net.proxy;

    if (net.useStaticIP) {
      netObj["staticIP"] = net.staticIP.toString();
      netObj["staticGateway"] = net.staticGateway.toString();
      netObj["staticSubnet"] = net.staticSubnet.toString();
      netObj["staticDNS"] = net.staticDNS.toString();
    }
  }

  String output;
  serializeJson(doc, output);
  return output;
}

bool Settings::deserializeSettings(JsonObject doc, WiFiSettings& settings) {
  settings.isWifiTurnedOn = doc["isWifiTurnedOn"] | true;
  settings.currentIdNetworkSetting = doc["currentIdNetworkSetting"] | -1;
  settings.isAP = doc["isAP"] | true;
  settings.ssidAP = doc["ssidAP"] | "ESP-AP";
  settings.mDNS = doc["mDNS"] | "sd";
  settings.passwordAP = doc["passwordAP"] | "";
  settings.autoReconnect = doc["autoReconnect"] | true;
  settings.timeZone = doc["timeZone"] | 3;
  settings.saveLogs = doc["saveLogs"] | true;

  if (doc.containsKey("staticIpAP")) {
    settings.staticIpAP.fromString(doc["staticIpAP"] | "192.168.1.1");
  }

  if (doc.containsKey("telegramSettings")) {
    JsonObject telegram = doc["telegramSettings"];
    settings.telegramSettings.isTelegramOn = telegram["isTelegramOn"] | true;
    settings.telegramSettings.botId = telegram["botId"] | "";
    settings.telegramSettings.lastMessage = telegram["lastMessage"] | "";

        settings.telegramSettings.isPush[0] = false;
    settings.telegramSettings.isPush[1] = false;
    settings.telegramSettings.isPush[2] = false;

    if (telegram.containsKey("isPush")) {
      JsonArray isPushJsonArray = telegram["isPush"];

      for (int i = 0; i < 3 && i < isPushJsonArray.size(); i++) {
        settings.telegramSettings.isPush[i] = isPushJsonArray[i];
      }
    }

    settings.telegramSettings.telegramUsers.clear();
    if (telegram.containsKey("telegramUsers")) {
      JsonArray usersArray = telegram["telegramUsers"];
      for (JsonObject user : usersArray) {
        TelegramUser tgUser;
        tgUser.id = user["id"] | "";
        tgUser.reading = user["reading"] | true;
        tgUser.writing = user["writing"] | true;
        settings.telegramSettings.telegramUsers.push_back(tgUser);
      }
    }
  }

  settings.networkSettings.clear();
  if (doc.containsKey("networkSettings")) {
    JsonArray networkArray = doc["networkSettings"];
    for (JsonObject net : networkArray) {
      NetworkSetting ns;
      ns.ssid = net["ssid"] | "";
      ns.bssid = net["bssid"] | "";
      ns.channel = net["channel"] | 0;
      ns.password = net["password"] | "";
      ns.useStaticIP = net["useStaticIP"] | false;
      ns.useProxy = net["useProxy"] | false;
      ns.proxy = net["proxy"] | "";

      if (ns.useStaticIP) {
        ns.staticIP.fromString(net["staticIP"] | "0.0.0.0");
        ns.staticGateway.fromString(net["staticGateway"] | "0.0.0.0");
        ns.staticSubnet.fromString(net["staticSubnet"] | "0.0.0.0");
        ns.staticDNS.fromString(net["staticDNS"] | "0.0.0.0");
      }
      settings.networkSettings.push_back(ns);
    }
  }

  return true;
}

bool Settings::loadSettings() {
  if (!loadSettingsFromFile("/settings.json")) {
    Serial.println("Failed to load settings from file, using defaults");
    return loadDefaults(true);
  }
  Serial.println("Settings loaded from file successfully");
  return true;
}

bool Settings::loadSettingsFromFile(const char* filename) {
  if (!isFSMounted()) return false;

  if (!SPIFFS.exists(filename)) {
    return false;
  }

  File file = SPIFFS.open(filename, "r");
  if (!file) {
    return false;
  }

  DynamicJsonDocument doc(4096);
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error) {
    Serial.println("Failed to parse settings file: " + String(error.c_str()));
    return false;
  }

  return deserializeSettings(doc.as<JsonObject>(), ws);
}

bool Settings::saveSettings() {

  File file = SPIFFS.open("/settings.json", "w");

  if (!file) {
    Serial.println("Ошибка открытия файла для записи");
    return false;
  }

   String jsonString = serializeSettings(ws);
   yield();
   file.print(jsonString);

  file.close();
  return true;

}

void Settings::format() {
  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_OFF);
  delay(100);

  SPIFFS.format();
  delay(500);
  ESP.restart();
}
