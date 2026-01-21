#pragma once

#include <vector>
#include <IPAddress.h>
#include <WString.h>
#include "AppState.h"

#include "CommonTypes.h"

struct TelegramUser {
  String id;
  bool reading;
  bool writing;
};

struct TelegramSettings {
  bool isTelegramOn;
  String botId;
  std::vector<TelegramUser> telegramUsers;
  String lastMessage;
  bool isPush[3];
};

struct NetworkSetting {
  String ssid;
  String bssid;
  uint8_t channel;
  String password;
  int signalLevel;
  bool useStaticIP;
  IPAddress staticIP;
  IPAddress staticGateway;
  IPAddress staticSubnet;
  IPAddress staticDNS;
  bool useProxy;
  String proxy;
};

struct WiFiSettings {
  bool isWifiTurnedOn = true;
  int currentIdNetworkSetting;
  std::vector<NetworkSetting> networkSettings;
  String ssidAP;
  String passwordAP;
  String mDNS;
  IPAddress staticIpAP;
  bool isAP;
  bool isTemporaryAP;
  bool autoReconnect;
  String urlPath;

  int8_t timeZone = 3;
  bool saveLogs;
  TelegramSettings telegramSettings;
  int8_t systemLoading;
};

class Settings {
public:
    Settings(AppState& appState);

    bool loadSettings();
    bool saveSettings();
    bool loadDefaults(bool saveToFile = false);

    bool isFSMounted();
    void printFsInfo();
    bool freeSpaceFS();
    void format();
    void begin();

    WiFiSettings ws;

private:
    AppState& appState;
    bool spiffsMounted = false;

    String serializeSettings(const WiFiSettings& settings);

    bool deserializeSettings(JsonObject doc, WiFiSettings& settings);

    bool loadSettingsFromFile(const char* filename);
    bool saveSettingsToFile(const char* filename, const String& jsonString);

};
