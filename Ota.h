#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <WiFiClientSecure.h>

#include "CommonTypes.h"

#ifdef ESP32
#include <AsyncTCP.h>
#include <HTTPClient.h>
#include <Update.h>
#elif defined(ESP8266)
#include <ESP8266HTTPClient.h>
#include <Updater.h>
#endif

#include <ArduinoJson.h>
#include "Logger.h"
#include "AppState.h"
#include "ConfigSettings.h"

class Ota {
  public:
    Ota(Settings& settings, Logger& logger, AppState& appState);

    void handleFileUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *fileData, size_t len, bool final);
    void loop();

    bool isUpdate;

    enum ProcessState {
      STATE_IDLE,
      STATE_CONNECTING,
      STATE_READING_HEADERS,
      STATE_DOWNLOADING,
      STATE_COMPLETE,
      STATE_ERROR
    };

#ifdef ESP32
    static constexpr uint32_t MIN_FREE_MEMORY = 6 * 1024;
#elif defined(ESP8266)
    static constexpr uint32_t MIN_FREE_MEMORY = 6 * 1024;
#endif

    struct DownloadState {
      ProcessState state = STATE_IDLE;
      int attempt = 0;
      int totalReceived = 0;
      int contentLength = 0;
      bool otaStarted = false;
      bool isFirmware = false;
      unsigned long startTime = 0;
      unsigned long lastProgressTime = 0;

      String host = "";
      String path = "";
      String fileName = "";

File file;

#ifdef ESP8266
BearSSL::WiFiClientSecure client;
#elif defined(ESP32)
WiFiClientSecure client;
#endif

      DownloadState() {
#ifdef ESP8266
        client.setBufferSizes(1024, 512);
#endif
        client.setInsecure();
        client.setTimeout(180000);
      }

      void cleanup() {
        client.stop();
        delay(100);
        client.flush();
        if (file) file.close();
        delay(50);

        state = STATE_IDLE;
        totalReceived = 0;
        contentLength = 0;
        otaStarted = false;
        isFirmware = false;
        startTime = 0;
        lastProgressTime = 0;
        host = "";
        path = "";
        fileName = "";

#ifdef ESP8266
        client.setBufferSizes(0, 0);
        delay(50);
        client.setBufferSizes(1024, 512);
#endif
      }
    };

    DownloadState dlState;

    enum ProcessStatus {
      HTTP_UPDATE_FAILED,
      HTTP_UPDATE_OK,
      HTTP_UPDATE_END,
      HTTP_FILE_END,
      HTTP_FILE_FAILED,
      HTTP_DOWNLOAD_IN_PROGRESS,
      HTTP_IDLE,
      HTTP_CONNECTING,
      HTTP_DOWNLOADING
    };

    ProcessStatus handleUpdate(const String& file_path, const String& fileName, bool debug);

  private:
    Settings& settings;
    Logger& logger;
    AppState& appState;
    String statusUpdate;
    String previousStatus;
    bool statusUpdateChanged;
    String getFileNameFromUrl(String url);

    unsigned long _lastDataReceivedTime = 0;
    const unsigned long NO_DATA_TIMEOUT = 10000;
};
