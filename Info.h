#ifndef INFO__H
#define INFO__H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "CommonTypes.h"
#include "ConfigSettings.h"

#ifdef ESP32
#include <WiFi.h>
#include <esp_system.h>
#include <esp_chip_info.h>
#include <esp_idf_version.h>
#include <SPIFFS.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <FS.h>
#endif

constexpr size_t SYSTEM_REPORT_BUFFER_SIZE = 4096;

struct SystemReport {

    String chipModel;
    uint32_t cpuFreqMhz;
    uint32_t totalHeapKb;
    uint32_t freeHeapKb;
    uint32_t minFreeHeapKb;
    uint32_t maxAllocHeapKb;
    uint8_t heapFragmentation;
    int8_t systemLoading;

    uint32_t totalSpiffsKb;
    uint32_t usedSpiffsKb;
    uint32_t freeSpiffsKb;

    WiFiMode_t wifiMode;
    String wifiSsid;
    String wifiIp;
    String wifiGateway;
    String wifiSubnet;
    String wifiDns;
    int32_t wifiRssi;
    String wifiMac;

    String apSsid;
    String apIp;
    String apMac;
    uint8_t apStationsCount;
};

class Info
{
public:

    Info(Settings& settings);

    void getSystemReport(SystemReport& report);
    String getSystemStatus();
    String getChipModel();

    size_t formatReport(const SystemReport& report, char* buffer, size_t bufferSize);

private:

    Settings& settings;

};

#endif
