/*
  # Форматируем
  prettier --print-width 120 --html-whitespace-sensitivity css --write compressed.html
  Далее сжатие gzip -k -9 index.html
  Перевод .gz в .h - % xxd -i index.html.gz > index_html_gz.h

  #ifndef INDEX_HTML_GZ_H
  #define INDEX_HTML_GZ_H

  #include <stdint.h>

  static const uint8_t index_html_gz[] PROGMEM = {
  ... 0x00
  };

  static const unsigned int index_html_gz_len = sizeof(index_html_gz);

  #endif
*/


#include "WiFiManager.h"
#include "WebServer.h"
#include "TelegramBot.h"
#include "ConfigSettings.h"
#include "TimeModule.h"
#include "DeviceManager.h"
#include "Info.h"
#include "Ota.h"
#include "build_flags.h"
#include "Log.h"
#include "AppState.h"
#include "Control.h"
#include <EEPROM.h> 
#include <esp_task_wdt.h>
#include "esp_err.h"

#if defined(ESP8266)
// Для ESP-01 и других ESP8266 плат
#if defined(ARDUINO_ESP8266_ESP01) || defined(ESP01)
#define LED_PIN 2  // GPIO2 для ESP-01 (синий светодиод)
#else
#define LED_PIN 2  // По умолчанию для большинства ESP8266
#endif
#elif defined(ESP32)
// Для ESP32-S2 и других ESP32 вариаций
#if defined(CONFIG_IDF_TARGET_ESP32S2) || defined(ARDUINO_ESP32S2_DEV)
#define LED_PIN 15 // GPIO15 для ESP32-S2
#else
#define LED_PIN 2  // По умолчанию для других ESP32
#endif
#endif

#define EEPROM_SIZE 512

#define BOOT_STATE_ADDRESS 0     
#define TIME_STORAGE_ADDRESS 32   

#define MAX_BOOT_ATTEMPTS 3
#define STABLE_UPTIME_MS 10000 // 10 секунд

#define CONTROL_BUTTON
#define LONG_PRESS_TIME 5000 // 5 секунд
#define buttonPin 5

#define LOGGING_REBOOT

unsigned long buttonPressStartTime = 0;
bool isButtonCurrentlyPressed = false;

struct BootState {
  unsigned int bootCount = 0;
  bool bootSuccess = false;
  bool wasFormattedDueToBootLoop = false;
};

AppState appState;
Settings configSettings(appState);
Info sysInfo(configSettings); 
Logger logger;

TimeModule timeModule(logger, appState, configSettings);
Ota ota(configSettings, logger, appState);
DeviceManager deviceManager;
Control control(deviceManager, logger);

WiFiManager wifiManager(configSettings, timeModule, logger, appState);
WebServer webServer(wifiManager, configSettings, deviceManager, timeModule, sysInfo, ota, logger, appState);
TelegramBot telegramBot(configSettings, webServer, logger, appState, ota, sysInfo, deviceManager);

// === SETUP ===

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n--- ESP Boot ---");

 #ifdef ESP32
   esp_task_wdt_config_t twdt_config = {
    .timeout_ms = 10000,     
    .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
    .trigger_panic = true   
  };

  ESP_ERROR_CHECK(esp_task_wdt_reconfigure(&twdt_config));
  
#endif

#ifdef LOGGING_REBOOT

char strBuffer[100]; 

esp_reset_reason_t reason = esp_reset_reason();

const char* reasonText;
switch(reason) {
    case ESP_RST_POWERON:    reasonText = "Первое включение"; break;
    case ESP_RST_EXT:        reasonText = "Внешний сброс (кнопка)"; break;
    case ESP_RST_SW:         reasonText = "Программный сброс"; break;
    case ESP_RST_PANIC:      reasonText = "КРИТИЧЕСКАЯ ОШИБКА (panic)"; break;
    case ESP_RST_INT_WDT:    reasonText = "WATCHDOG: Прерывание"; break;
    case ESP_RST_TASK_WDT:   reasonText = "WATCHDOG: Задача зависла"; break;
    case ESP_RST_WDT:        reasonText = "WATCHDOG: Другая причина"; break;
    case ESP_RST_DEEPSLEEP:  reasonText = "Выход из глубокого сна"; break;
    case ESP_RST_BROWNOUT:   reasonText = "ПРОБЛЕМА ПИТАНИЯ: Brownout"; break;
    case ESP_RST_SDIO:       reasonText = "Сброс по SDIO"; break;
    default:                 reasonText = "Неизвестная причина"; break;
}

snprintf(strBuffer, sizeof(strBuffer), "Причина перезагрузки: %s [%d]", reasonText, reason);

logger.addLog(strBuffer);
delay(1000);

#endif

#ifdef CONTROL_BUTTON
  
  pinMode(buttonPin, INPUT_PULLUP);
  Serial.println("Checking for control button press to format...");

 
  if (digitalRead(buttonPin) == LOW) {
    Serial.println("Button is held. Starting 5-second timer for formatting...");
    unsigned long startTime = millis();

    while (digitalRead(buttonPin) == LOW) {
      if (millis() - startTime > LONG_PRESS_TIME) {
        Serial.println("5 seconds have passed. Formatting SPIFFS and restarting...");

        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        delay(100);

        SPIFFS.format();
        Serial.println("SPIFFS formatted.");

        ESP.restart();
      }

      delay(10);
    }

    Serial.println("Button was released before 5 seconds. Continuing normal startup.");
  }
#endif

  EEPROM.begin(EEPROM_SIZE);


  BootState state;
  EEPROM.get(BOOT_STATE_ADDRESS, state);

  if (!state.bootSuccess) {
    state.bootCount++;
    Serial.printf("Предыдущий запуск был неудачным. Увеличиваем счетчик до: %d\n", state.bootCount);
  } else {
    Serial.println("Предыдущий запуск был успешным. Сбрасываем счетчик.");
    state.bootCount = 0;
  }

  state.bootSuccess = false; 

  if (state.bootCount > MAX_BOOT_ATTEMPTS) {
    Serial.println("!!! ОБНАРУЖЕНА ЦИКЛИЧЕСКАЯ ПЕРЕЗАГРУЗКА !!!");
    Serial.println("Устанавливаем флаг для форматирования SPIFFS...");

    state.wasFormattedDueToBootLoop = true;
    state.bootCount = 0;
    state.bootSuccess = false;

    EEPROM.put(BOOT_STATE_ADDRESS, state);
    EEPROM.commit(); 
    appState.isFormat = true;
   
  } else {
    EEPROM.put(BOOT_STATE_ADDRESS, state);

  }
  // =================================================================


#ifndef ESP32
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);
  digitalWrite(LED_PIN, HIGH);
  delay(300);
  digitalWrite(LED_PIN, LOW);
#endif

  configSettings.begin();
  Serial.printf("Free heap befor LoadSetting: %d\n", ESP.getFreeHeap());

  if (!configSettings.loadSettings()) {
    Serial.println("Using default settings");
  }
  Serial.printf("Free heap after LoadSetting: %d\n", ESP.getFreeHeap());

  deviceManager.deviceInit();

  if (configSettings.ws.isWifiTurnedOn) {
    wifiManager.begin();
    unsigned long startTime = millis();
    while (!wifiManager.wasConnected && millis() < (startTime + 5000)) {
      wifiManager.loop();
      delay(10);
    }
  } else {
#ifdef ESP32
    if (WiFi.getMode() != WIFI_MODE_NULL) {
      WiFi.disconnect(true);
      WiFi.mode(WIFI_MODE_NULL);
      delay(100);
    }
#elif defined(ESP8266)
    if (WiFi.getMode() != WIFI_OFF) {
      WiFi.disconnect(true);
      WiFi.mode(WIFI_OFF);
      delay(100);
    }
#endif
  }

  timeModule.beginNTP();
  if (!WiFi.isConnected()) {
    timeModule.updateTime();
  }

  if (configSettings.ws.isWifiTurnedOn) {
    webServer.begin();
    Serial.printf("Free heap after web sever: %d\n", ESP.getFreeHeap());
  }

  control.setup();

#ifndef ESP32
  digitalWrite(LED_PIN, LOW);
  delay(1000);
  digitalWrite(LED_PIN, HIGH);
#endif

  logger.setLoggingEnabled(configSettings.ws.saveLogs);
  logger.loadLogsFromSPIFFS();
  logger.addLog("System started");

  EEPROM.commit();
  Serial.println("Setup finished, EEPROM committed.");

   logger.forceSave();
}


void loop() {

  static bool bootSuccessHandled = false;
  if (!bootSuccessHandled && millis() > STABLE_UPTIME_MS) {
    bootSuccessHandled = true;
    BootState state;
    EEPROM.get(BOOT_STATE_ADDRESS, state);

    if (state.wasFormattedDueToBootLoop) {
      logger.addLog("ВНИМАНИЕ: Обнаружена циклическая перезагрузка. Настройки были сброшены для восстановления.");
      Serial.println("Сообщили в лог о предыдущем форматировании.");
      state.wasFormattedDueToBootLoop = false;
    }

    Serial.println("Система работает стабильно. Запуск считаем успешным.");
    state.bootSuccess = true;
    state.bootCount = 0;

    EEPROM.put(BOOT_STATE_ADDRESS, state);
    EEPROM.commit(); 
    
  if (appState.isFormat) {
    configSettings.ws.isWifiTurnedOn = false;
    configSettings.format();
    return;
  }


  unsigned long loopStartTime = micros();

  if (deviceManager.isSaveControl && !ota.isUpdate) {
    delay(10);
    deviceManager.writeDevicesToFile(deviceManager.myDevices, "/devices.json");
    deviceManager.isSaveControl = false;
  }

  if (!ota.isUpdate && !deviceManager.isSaveControl && !appState.isStartWifi && !webServer.saveNetwork.isSaveNetwork && !wifiManager.isScanning && appState.connectState == AppState::CONNECT_IDLE && !appState.isFormat) {
    control.loop();
  }

  if (configSettings.ws.isWifiTurnedOn) {
    webServer.loop();
    wifiManager.loop();
    ota.loop();

    if (configSettings.ws.telegramSettings.isTelegramOn &&
        !configSettings.ws.isAP &&
        !configSettings.ws.isTemporaryAP &&
        !wifiManager.isScanning &&
        appState.connectState == AppState::CONNECT_IDLE &&
        !ota.isUpdate &&
        timeModule.isInternetAvailable &&
        !wifiManager.isReconnecting() &&
        //!appState.isStartWifi &&
        !deviceManager.isSaveControl) {
      telegramBot.loop();
    }
  }

  #ifndef CONTRLOL_BUTTON

  bool isPress = control.checkTouchSensor(buttonPin);

  if (isPress && !isButtonCurrentlyPressed) {
    Serial.println("Button pressed");
    buttonPressStartTime = millis(); 
  }

  if (!isPress && isButtonCurrentlyPressed) {

    unsigned long pressDuration = millis() - buttonPressStartTime;

    if (pressDuration > LONG_PRESS_TIME) {
      Serial.println("Long press detected! Toggling WiFi state and restarting...");
      configSettings.ws.isWifiTurnedOn = !configSettings.ws.isWifiTurnedOn;


      if (configSettings.saveSettings()) {
        Serial.println("Settings saved successfully.");
      } else {
        Serial.println("ERROR: Failed to save settings!");
      }

      ESP.restart();
    }
  }

  isButtonCurrentlyPressed = isPress;

  #endif

  unsigned long loopDuration = micros() - loopStartTime;


  unsigned long totalCycleTime = loopDuration + 10000;


  int8_t calculatedLoad = (int8_t)((loopDuration * 100) / totalCycleTime);

  if (calculatedLoad > 100) {
    calculatedLoad = 100;
  }

  configSettings.ws.systemLoading = calculatedLoad;

  delay(10);
}
