#include "Info.h"

Info::Info(Settings& settings) : settings(settings)
{

}

String Info::getChipModel() {
#ifdef ESP32
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    switch (chip_info.model) {
        case CHIP_ESP32:    return "ESP32";
        case CHIP_ESP32S2:  return "ESP32-S2";
        case CHIP_ESP32S3:  return "ESP32-S3";
        case CHIP_ESP32C3:  return "ESP32-C3";
        case CHIP_ESP32C6:  return "ESP32-C6";
        default: {
            char unknown_model[20];
            snprintf(unknown_model, sizeof(unknown_model), "Unknown (0x%X)", chip_info.model);
            return String(unknown_model);
        }
    }
#elif defined(ESP8266)
    return "ESP8266";
#else
    return "Unknown";
#endif
}

void Info::getSystemReport(SystemReport& report) {

    report.chipModel = getChipModel();

    report.systemLoading = settings.ws.systemLoading;

#ifdef ESP32
    report.cpuFreqMhz = getCpuFrequencyMhz();
    report.totalHeapKb = ESP.getHeapSize() / 1024;
    report.freeHeapKb = ESP.getFreeHeap() / 1024;
    report.minFreeHeapKb = ESP.getMinFreeHeap() / 1024;
    report.maxAllocHeapKb = ESP.getMaxAllocHeap() / 1024;
#elif defined(ESP8266)
    report.cpuFreqMhz = ESP.getCpuFreqMHz();
    uint32_t freeHeap;
    uint16_t maxFreeBlock;
    uint8_t heapFragmentation;
    ESP.getHeapStats(&freeHeap, &maxFreeBlock, &heapFragmentation);

    report.totalHeapKb = 80;
    report.freeHeapKb = freeHeap / 1024;
    report.heapFragmentation = heapFragmentation;
#endif

#ifdef ESP32
    size_t totalBytes = SPIFFS.totalBytes();
    size_t usedBytes = SPIFFS.usedBytes();
    report.totalSpiffsKb = totalBytes / 1024;
    report.usedSpiffsKb = usedBytes / 1024;
    report.freeSpiffsKb = (totalBytes - usedBytes) / 1024;
#elif defined(ESP8266)
    FSInfo fs_info;
    if (SPIFFS.info(fs_info)) {
        report.totalSpiffsKb = fs_info.totalBytes / 1024;
        report.usedSpiffsKb = fs_info.usedBytes / 1024;
        report.freeSpiffsKb = (fs_info.totalBytes - fs_info.usedBytes) / 1024;
    } else {
        report.totalSpiffsKb = 0;
        report.usedSpiffsKb = 0;
        report.freeSpiffsKb = 0;
    }
#endif

    report.wifiMode = WiFi.getMode();

    if (report.wifiMode == WIFI_STA || report.wifiMode == WIFI_AP_STA) {
        if (WiFi.status() == WL_CONNECTED) {
            report.wifiSsid = WiFi.SSID();
            report.wifiIp = WiFi.localIP().toString();
            report.wifiGateway = WiFi.gatewayIP().toString();
            report.wifiSubnet = WiFi.subnetMask().toString();
            report.wifiDns = WiFi.dnsIP(0).toString();
            report.wifiRssi = WiFi.RSSI();
        }
        uint8_t mac[6];
        WiFi.macAddress(mac);
        char macStr[18];
        snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        report.wifiMac = String(macStr);
    }

    if (report.wifiMode == WIFI_AP || report.wifiMode == WIFI_AP_STA) {
        report.apSsid = WiFi.softAPSSID();
        report.apIp = WiFi.softAPIP().toString();
        report.apStationsCount = WiFi.softAPgetStationNum();
        uint8_t mac[6];
        WiFi.softAPmacAddress(mac);
        char macStr[18];
        snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        report.apMac = String(macStr);
    }
}

size_t Info::formatReport(const SystemReport& report, char* buffer, size_t bufferSize) {
    size_t offset = 0;

    offset += snprintf(buffer + offset, bufferSize - offset,
        "Системная информация\n\n"
        "Процессор и память:\n"
        "  Модель: %s\n"
        "  Частота: %d MHz\n"
        "  Загрузка системы: %d%%\n"
        "  Всего ОЗУ: %d KB\n"
        "  Свободно ОЗУ: %d KB\n",
        report.chipModel.c_str(),
        report.cpuFreqMhz,
        report.systemLoading,
        report.totalHeapKb,
        report.freeHeapKb
    );

#ifdef ESP32
    offset += snprintf(buffer + offset, bufferSize - offset,
        "  Минимум свободной ОЗУ: %d KB\n"
        "  Макс. блок для выделения: %d KB\n",
        report.minFreeHeapKb,
        report.maxAllocHeapKb
    );
#elif defined(ESP8266)
    offset += snprintf(buffer + offset, bufferSize - offset,
        "  Фрагментация кучи: %d%%\n",
        report.heapFragmentation
    );
#endif

    offset += snprintf(buffer + offset, bufferSize - offset,
        "\nФайловая система (SPIFFS):\n"
        "  Всего: %d KB\n"
        "  Использовано: %d KB\n"
        "  Свободно: %d KB\n\n",
        report.totalSpiffsKb,
        report.usedSpiffsKb,
        report.freeSpiffsKb
    );

    offset += snprintf(buffer + offset, bufferSize - offset,
        "Сеть Wi-Fi:\n"
        "  Режим: %s\n",
        (report.wifiMode == WIFI_AP) ? "Точка доступа (AP)" :
        (report.wifiMode == WIFI_STA) ? "Клиент (STA)" :
        (report.wifiMode == WIFI_AP_STA) ? "Точка доступа + Клиент (AP+STA)" : "Выключен"
    );

    if (report.wifiMode == WIFI_STA || report.wifiMode == WIFI_AP_STA) {
        if (WiFi.status() == WL_CONNECTED) {
            offset += snprintf(buffer + offset, bufferSize - offset,
                "  SSID: %s\n"
                "  IP: %s\n"
                "  Шлюз: %s\n"
                "  Маска: %s\n"
                "  DNS: %s\n"
                "  RSSI: %d dBm\n"
                "  MAC: %s\n",
                report.wifiSsid.c_str(),
                report.wifiIp.c_str(),
                report.wifiGateway.c_str(),
                report.wifiSubnet.c_str(),
                report.wifiDns.c_str(),
                report.wifiRssi,
                report.wifiMac.c_str()
            );
        } else {
            offset += snprintf(buffer + offset, bufferSize - offset,
                "  Подключение: отсутствует\n"
                "  MAC: %s\n",
                report.wifiMac.c_str()
            );
        }
    }

    if (report.wifiMode == WIFI_AP || report.wifiMode == WIFI_AP_STA) {
        offset += snprintf(buffer + offset, bufferSize - offset,
            "\nТочка доступа (AP):\n"
            "  SSID: %s\n"
            "  IP: %s\n"
            "  MAC: %s\n"
            "  Подключено клиентов: %d\n",
            report.apSsid.c_str(),
            report.apIp.c_str(),
            report.apMac.c_str(),
            report.apStationsCount
        );
    }

    offset += snprintf(buffer + offset, bufferSize - offset,
        "\n\n"
        "Kolibri home v1.2.0\n"
        "Dolgopolov Pavel, 2025\n"
        "dl.pavel@mail.ru"
    );

    return offset;
}

String Info::getSystemStatus() {
    SystemReport report;
    getSystemReport(report);

    char* reportBuffer = (char*)malloc(SYSTEM_REPORT_BUFFER_SIZE);
    if (!reportBuffer) {
        Serial.println("[INFO] FATAL: Failed to allocate buffer for system report!");
        return "Ошибка: нехватка памяти для формирования отчета.";
    }

    size_t reportLen = formatReport(report, reportBuffer, SYSTEM_REPORT_BUFFER_SIZE);
    reportBuffer[reportLen] = '\0';

    String result = String(reportBuffer);
    free(reportBuffer);

    return result;
}
