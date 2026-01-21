#include "Ota.h"

volatile bool otaUpdateCompleted = false;
unsigned long otaRebootTime = 0;

Ota::Ota(Settings& settings, Logger& logger, AppState& appState)
  : settings(settings),
    logger(logger),
    appState(appState),
    isUpdate(false),
    statusUpdate(""),
    previousStatus(""),
    statusUpdateChanged(false)
{
}

void Ota::handleFileUpload(AsyncWebServerRequest* request, String filename, size_t index, uint8_t* fileData, size_t len, bool final) {
  static bool isUpdateFlash = false;
  static size_t totalSize = 0;
  static bool updateFailed = false;

  bool isFirmware = filename.endsWith(".bin");

  if (index == 0) {
    totalSize = 0;
    updateFailed = false;

#ifdef ESP8266
    Update.runAsync(true);
#endif

    if (!isFirmware && settings.freeSpaceFS()) {
      dlState.file = SPIFFS.open("/" + filename, "w");
      if (!dlState.file) {
        request->send(500, "text/plain", "File open error");
        return;
      }
      isUpdateFlash = false;
    } else {
#ifdef ESP32
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
#elif defined(ESP8266)
      uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
      if (!Update.begin(maxSketchSpace, U_FLASH)) {
#endif
        request->send(500, "text/plain", "OTA Begin Failed");
        updateFailed = true;
        return;
      }
      isUpdate = true;
      isUpdateFlash = true;
    }
  }

  if (updateFailed) {
    isUpdate = false;
    return;
  }

  if (len > 0) {
    totalSize += len;

    if (isUpdateFlash) {
      if (Update.write(fileData, len) != len) {
        request->send(500, "text/plain", "OTA Write Error");
        isUpdate = false;
#ifdef ESP32
        Update.abort();
#endif
        return;
      }
    } else if (dlState.file) {
      if (dlState.file.write(fileData, len) != len) {
        request->send(500, "text/plain", "SPIFFS Write Error");
        dlState.file.close();
        return;
      }
    }
  }

  if (final) {
    if (isUpdateFlash && !updateFailed) {
      if (Update.end(true)) {
        request->send(200, "text/plain", "OTA Complete");
        otaUpdateCompleted = true;
        otaRebootTime = millis();
      } else {
        request->send(500, "text/plain", "OTA End Failed");
      }
    } else if (dlState.file && !updateFailed) {
      dlState.file.close();
      request->send(200, "text/plain", "File Uploaded");
    }
    isUpdate = false;
  }
}

void Ota::loop() {
  if (otaUpdateCompleted && (millis() - otaRebootTime > 500)) {
    ESP.restart();
  }
}

String Ota::getFileNameFromUrl(String url) {
  int lastSlash = url.lastIndexOf('/');
  return (lastSlash == -1) ? "firmware.bin" : url.substring(lastSlash + 1);
}

Ota::ProcessStatus Ota::handleUpdate(const String & file_path, const String & fileName, bool debug) {
  const int MAX_ATTEMPTS = 3;
  const unsigned long TIMEOUT = 300000;

  auto checkMemory = [this, debug]() -> ProcessStatus {
    uint32_t freeMemory = ESP.getFreeHeap();
#ifdef ESP8266
    if (freeMemory < MIN_FREE_MEMORY || ESP.getHeapFragmentation() > 50) {
#elif defined(ESP32)
    if (freeMemory < MIN_FREE_MEMORY || ESP.getMinFreeHeap() < MIN_FREE_MEMORY / 2) {
#endif
      if (debug) {
        Serial.printf("❌ Not enough memory: %u bytes (need %u bytes)\n",
        freeMemory, MIN_FREE_MEMORY);
        Serial.println("🔄 Restarting due to low memory...");
      }
      delay(1000);
      ESP.restart();
      return HTTP_UPDATE_FAILED;
    }
    return HTTP_DOWNLOAD_IN_PROGRESS;
  };

  if (!dlState.isFirmware && !SPIFFS.exists("/")) {
    if (!SPIFFS.begin()) {
      dlState.cleanup();
      return HTTP_FILE_FAILED;
    }
  }

  if (dlState.attempt == 0) {
    if (debug) {
      Serial.printf("📡 Starting download process:\n");
      Serial.printf("   URL: '%s'\n", file_path.c_str());
      Serial.printf("   File name: '%s'\n", fileName.c_str());
    }

    dlState.otaStarted = false;
    dlState.file = File();

    int protocolIndex = file_path.indexOf("://");
    if (protocolIndex == -1) {
      if (debug) Serial.println("❌ Invalid URL format");
      return HTTP_UPDATE_FAILED;
    }

    int hostStart = protocolIndex + 3;
    int hostEnd   = file_path.indexOf('/', hostStart);
    if (hostEnd == -1) {
      dlState.host = file_path.substring(hostStart);
      dlState.path = "/";
    } else {
      dlState.host = file_path.substring(hostStart, hostEnd);
      dlState.path = file_path.substring(hostEnd);
    }

    if (fileName.isEmpty()) {

      int lastSlash = dlState.path.lastIndexOf('/');
      if (lastSlash != -1) {
        dlState.fileName = dlState.path.substring(lastSlash + 1);
      } else {
        dlState.fileName = "download";
      }
    } else {
      dlState.fileName = fileName;
    }

    dlState.isFirmware = dlState.fileName.endsWith(".bin");

    if (debug) {
      Serial.printf("🌍 Host: %s\n📄 Path: %s\n💾 File: %s (%s)\n",
                    dlState.host.c_str(), dlState.path.c_str(),
                    dlState.fileName.c_str(),
                    dlState.isFirmware ? "Firmware" : "Data");
    }
  }

  switch (dlState.state) {
    case STATE_IDLE:

      dlState.attempt++;

      dlState.state = STATE_CONNECTING;
      dlState.startTime = millis();
      return HTTP_DOWNLOAD_IN_PROGRESS;

    case STATE_CONNECTING:

      dlState.client.setInsecure();
      dlState.client.setTimeout(180000);
      if (dlState.client.connect(dlState.host.c_str(), 443)) {
        if (debug) Serial.println("✅ Connected to server");

        String requestStr = "GET " + dlState.path + " HTTP/1.1\r\n" +
                            "Host: " + dlState.host + "\r\n" +
                            "User-Agent: ESP8266/ESP32\r\n" +
                            "Connection: close\r\n\r\n";

        dlState.client.print(requestStr);
        dlState.state = STATE_READING_HEADERS;
      } else {
        if (debug) Serial.println("❌ Connection failed");

        dlState.attempt++;
        if (dlState.attempt >= MAX_ATTEMPTS) {
          if (debug) Serial.printf("💥 Max connection attempts (%d) reached.\n", MAX_ATTEMPTS);
          dlState.state = STATE_ERROR;
        } else {
          if (debug) Serial.printf("⚠️ Will retry in 2 seconds...\n");
          dlState.state = STATE_IDLE;
        }
      }
      break;

    case STATE_READING_HEADERS: {
        while (dlState.client.available()) {
          String line = dlState.client.readStringUntil('\n');
          line.trim();

          if (line.startsWith("HTTP/1.1")) {
            int code = line.substring(9, 12).toInt();
            if (debug) Serial.printf("HTTP code: %d\n", code);
            if (code != 200) {
              dlState.state = STATE_ERROR;
              break;
            }
          } else if (line.startsWith("Content-Length:")) {
            dlState.contentLength = line.substring(16).toInt();
            if (debug) Serial.printf("📦 Content-Length: %d bytes\n", dlState.contentLength);
          } else if (line == "") {
            if (debug) Serial.println("📥 Start of body");

            if (dlState.isFirmware && !dlState.otaStarted) {
              if (Update.begin(dlState.contentLength)) {
                dlState.otaStarted = true;
#ifdef ESP8266
                Update.runAsync(true);
#endif
                if (debug) Serial.println("🚀 OTA update started");
              } else {
                if (debug) Serial.println("❌ OTA begin failed");
                dlState.state = STATE_ERROR;
                break;
              }
            }
            dlState.state = STATE_DOWNLOADING;
            _lastDataReceivedTime = millis();
            break;
          }
        }

        if (millis() - dlState.startTime > 5000) {
          if (debug) Serial.println("⏰ Header timeout");
          dlState.state = STATE_ERROR;
        }
        return HTTP_DOWNLOAD_IN_PROGRESS;
      }

    case STATE_DOWNLOADING: {

#ifdef ESP8266
        int bufferSize = constrain(ESP.getFreeHeap() / 8, 256, 512);
#else
        int bufferSize = constrain(ESP.getFreeHeap() / 4, 512, 1024);
#endif
        uint8_t buffer[bufferSize];

        if (!dlState.isFirmware && !dlState.file) {
          if (SPIFFS.exists("/" + dlState.fileName)) SPIFFS.remove("/" + dlState.fileName);
          dlState.file = SPIFFS.open("/" + dlState.fileName, "w");
          if (!dlState.file) {
            if (debug) Serial.println("❌ Failed to open file for writing");
            dlState.state = STATE_ERROR;
          }
        }

        if (dlState.state == STATE_DOWNLOADING) {
          int bytesRemaining = dlState.contentLength - dlState.totalReceived;
          int bytesToRead = min(min(dlState.client.available(), bytesRemaining), bufferSize);

          if (bytesToRead > 0) {
            int bytesRead = dlState.client.readBytes(buffer, bytesToRead);
            if (bytesRead > 0) {
              if (dlState.isFirmware) {
                if (Update.write(buffer, bytesRead) != bytesRead) {
                  if (debug) Serial.println("❌ OTA write failed");
                  dlState.state = STATE_ERROR;
                }
              } else {
                if (dlState.file.write(buffer, bytesRead) != bytesRead) {
                  if (debug) Serial.println("❌ File write failed");
                  dlState.file.close();
                  SPIFFS.remove("/" + dlState.fileName);
                  dlState.state = STATE_ERROR;
                }
                if (dlState.totalReceived % 4096 == 0) dlState.file.flush();
              }
              dlState.totalReceived += bytesRead;
            }
            _lastDataReceivedTime = millis();
          } else {

            if (millis() - _lastDataReceivedTime > NO_DATA_TIMEOUT) {
              if (debug) Serial.println("❌ No data timeout. Connection stalled.");
              dlState.state = STATE_ERROR;
              break;
            }
          }

          if (debug && millis() - dlState.lastProgressTime >= 1000) {
            int progress = (dlState.contentLength > 0) ?
                           (dlState.totalReceived * 100) / dlState.contentLength : 0;
            Serial.printf("Progress: %d%% | Received: %d/%d bytes\n",
                          progress, dlState.totalReceived, dlState.contentLength);
            dlState.lastProgressTime = millis();
          }

          if (dlState.totalReceived >= dlState.contentLength) {
            if (!dlState.isFirmware && dlState.file) {
              dlState.file.close();
              if (debug) Serial.println("✅ File download completed");
            }
            dlState.state = STATE_COMPLETE;
          }

          if (millis() - dlState.startTime > TIMEOUT) {
            if (debug) Serial.println("❌ Download timeout");
            dlState.state = STATE_ERROR;
          }
        }
        return HTTP_DOWNLOAD_IN_PROGRESS;
      }

    case STATE_COMPLETE:
      if (dlState.isFirmware) {
        if (Update.end(true)) {
          if (debug) Serial.println("✅ Firmware update successful! Restarting...");
          delay(2000);
          ESP.restart();
          return HTTP_UPDATE_OK;
        } else {
          if (debug) Serial.println("❌ OTA finalization failed");
          dlState.state = STATE_ERROR;
          return HTTP_DOWNLOAD_IN_PROGRESS;
        }
      } else {
        if (debug) Serial.println("✅ File download completed successfully");
        return HTTP_FILE_END;
      }
      dlState.cleanup();
      checkMemory();

      break;

    case STATE_ERROR:

      if (debug) {
        Serial.printf("💥 ERROR State: isFirmware=%s, attempt=%d/%d, otaStarted=%s\n",
                      dlState.isFirmware ? "true" : "false",
                      dlState.attempt, MAX_ATTEMPTS,
                      dlState.otaStarted ? "true" : "false");
      }

      if (dlState.isFirmware) {
        if (debug) Serial.println("❌ Firmware update failed. Restarting...");
        delay(2000);
        ESP.restart();
        return HTTP_UPDATE_FAILED;
      }

      if (dlState.attempt < MAX_ATTEMPTS) {
        if (debug) Serial.printf("⚠️  Retrying... Attempt %d/%d\n", dlState.attempt, MAX_ATTEMPTS);
        delay(2000);
        dlState.state = STATE_IDLE;
        dlState.attempt = 0;
        return HTTP_DOWNLOAD_IN_PROGRESS;
      } else {
        if (debug) Serial.println("❌ File download failed after max attempts");
        dlState.cleanup();
        return HTTP_FILE_FAILED;
      }

      checkMemory();

      break;
  }

  return HTTP_DOWNLOAD_IN_PROGRESS;
}
