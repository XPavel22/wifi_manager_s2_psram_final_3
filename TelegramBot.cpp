#include "TelegramBot.h"

constexpr size_t MESSAGE_BUFFER_SIZE = 4096;
constexpr size_t JSON_BUFFER_SIZE = 1024;
const int MAX_PART_LENGTH = 3000;

TelegramBot::TelegramBot(Settings& ws, WebServer& webServer, Logger& logger, AppState& appState, Ota& ota, Info& sysInfo, DeviceManager& deviceManager)
  : settings(ws),
    webServer(webServer),
    logger(logger),
    appState(appState),
    ota(ota),
    sysInfo(sysInfo),
    deviceManager(deviceManager),
    isBegin(false),
    isStop(false),
    shouldStartDownload(false),
    isDownloading(false),
    doRestart(false),
    commandsNeedUpdate(false),
    isFirstStart(true),
#ifdef ESP8266
    certificate(telegram_cert),
#endif
    myBot(client)
{
  lastLogCheckTime = millis();
}

void TelegramBot::begin() {

  if (!settings.ws.telegramSettings.isTelegramOn) return;

  this->botToken = settings.ws.telegramSettings.botId.c_str();

  if (!isValidTokenFormat(botToken)) {
    isBegin = false;
    Serial.println("❌ Invalid bot token format");
    return;
  }

#ifdef ESP8266
  client.setSession(&session);
  client.setTrustAnchors(&certificate);
  client.setBufferSizes(1024, 1024);
#elif defined(ESP32)
  client.setCACert(telegram_cert);
#endif

  myBot.setUpdateTime(2000);
  myBot.setTelegramToken(botToken);
  Serial.printf("BotToken: %s\n", botToken);

  Serial.print("\nTest Telegram connection... ");
  if (myBot.begin()) {
    Serial.println("OK");
    isBegin = true;
    startupTime = millis();
    isColdStart = false;
    commandsNeedUpdate = true;
  } else {
    Serial.println("NOK");
    isBegin = false;
  }
}

void TelegramBot::sendSimpleStatus(int64_t chatId) {
  if (deviceManager.myDevices.empty()) {
    TBMessage msg;
    msg.chatId = chatId;
    myBot.sendMessage(msg, "❌ Устройства не настроены.");
    return;
  }

  constexpr size_t STATUS_BUFFER_SIZE = 4096;
  char messageBuffer[STATUS_BUFFER_SIZE];
  int offset = 0;

  const Device& currentDevice = deviceManager.myDevices[deviceManager.currentDeviceIndex];

  offset += snprintf(messageBuffer + offset, STATUS_BUFFER_SIZE - offset,
                     "📊 Текущий статус системы\n\n"
                     "📟 Устройство: %s\n\n",
                     currentDevice.nameDevice);

  int outputRelayCount = 0;
  for (size_t i = 0; i < currentDevice.relays.size(); ++i) {
    const auto& relay = currentDevice.relays[i];
    if (relay.isOutput) {
      outputRelayCount++;
      offset += snprintf(messageBuffer + offset, STATUS_BUFFER_SIZE - offset,
                         "%s | /on%d /off%d | %s | %s\n\n",
                         relay.description,
                         outputRelayCount,
                         outputRelayCount,
                         relay.statePin ? "✅ ВКЛ" : "❌ ВЫКЛ",
                         relay.manualMode ? "🔧 Ручной" : "🤖 Авто");
    }
  }

  offset += snprintf(messageBuffer + offset, STATUS_BUFFER_SIZE - offset,
                     "Сброс реле /resetmanual \n\n"
                     "⚙️ Настройки системы:\n"
                     "• /timers_on /timers_off — Таймеры [%s]\n"
                     "• /schedule_on /schedule_off — Расписания [%s]\n"
                     "• /temp_on /temp_off — Температурный контроль [%s]\n"
                     "• /sensors_on /sensors_off — Действия на сенсоры [%s]\n\n",
                     currentDevice.isTimersEnabled ? "✅ ВКЛ" : "❌ ВЫКЛ",
                     currentDevice.isScheduleEnabled ? "✅ ВКЛ" : "❌ ВЫКЛ",
                     currentDevice.temperature.isUseSetting ? "✅ ВКЛ" : "❌ ВЫКЛ",
                     currentDevice.isActionEnabled ? "✅ ВКЛ" : "❌ ВЫКЛ");

  offset += snprintf(messageBuffer + offset, STATUS_BUFFER_SIZE - offset,
                     "🔔 Уведомления в Telegram:\n"
                     "• /push_error_on /push_error_off — Ошибки [ERROR] [%s]\n"
                     "• /push_info_on /push_info_off — Информация [INFO] [%s]\n"
                     "• /push_user_on /push_user_off — Пользовательские [USER] [%s]\n\n",
                     settings.ws.telegramSettings.isPush[0] ? "✅ ВКЛ" : "❌ ВЫКЛ",
                     settings.ws.telegramSettings.isPush[1] ? "✅ ВКЛ" : "❌ ВЫКЛ",
                     settings.ws.telegramSettings.isPush[2] ? "✅ ВКЛ" : "❌ ВЫКЛ");

  offset += snprintf(messageBuffer + offset, STATUS_BUFFER_SIZE - offset,
                     "Быстрые команды:\n"
                     "/start - Обновить этот статус\n"
                     "/help - Справка\n"
                     "/reset - Перезагрузить устройство\n");

  TBMessage msg;
  msg.chatId = chatId;
  myBot.sendMessage(msg, messageBuffer);
}

void TelegramBot::sendHelpMessage(int64_t chatId) {
  String messageBuffer = "❓ <b>Справка по командам</b>\n\n"
                         "<b>Основные команды:</b>\n"
                         "• /start — Показать текущий статус\n"
                         "• /help — Показать это сообщение\n\n"
                         "<b>Управление реле:</b>\n"
                         "• /on1, /on2... — Включить реле 1, 2 и т.д.\n"
                         "• /off1, /off2... — Выключить реле 1, 2 и т.д.\n"
                         "• /resetmanual — Сбросить всё в авторежим\n\n"
                         "<b>Настройки системы:</b>\n"
                         "• /timers_on /timers_off — Включить/выключить таймеры\n"
                         "• /schedule_on /schedule_off — Включить/выключить расписания\n"
                         "• /temp_on /temp_off — Включить/выключить температурный контроль\n"
                         "• /sensors_on /sensors_off — Включить/выключить действия на сенсоры\n\n"
                         "<b>Уведомления в Telegram:</b>\n"
                         "• /push_error_on /push_error_off — Включить/выключить уведомления об ошибках\n"
                         "• /push_info_on /push_info_off — Включить/выключить информационные уведомления\n"
                         "• /push_user_on /push_user_off — Включить/выключить пользовательские уведомления\n\n"
                         "<b>Системные команды:</b>\n"
                         "• /reset — Перезагрузить устройство\n"
                         "• /update — Обновить файл или прошивку\n"
                         "• /get — Получить файл с устройства (/get log.txt)\n"
                         "• /newtoken &lt;token&gt; — Установить новый токен\n\n"
                         "📌 <i>Некоторые команды требуют прав доступа.</i>";

  TBMessage msg;
  msg.chatId = chatId;
  myBot.setFormattingStyle(AsyncTelegram2::FormatStyle::HTML);
  myBot.sendMessage(msg, messageBuffer);
  myBot.setFormattingStyle(AsyncTelegram2::FormatStyle::MARKDOWN);
}

bool TelegramBot::hasPermission(const String& userId, const String& permission) {
  for (const auto& user : settings.ws.telegramSettings.telegramUsers) {
    if (user.id == userId) {
      if (permission == "reading") return user.reading;
      if (permission == "writing") return user.writing;
    }
  }
  return false;
}

int TelegramBot::getOutputRelayNumber(size_t relayIndex) {
  const Device& currentDevice = deviceManager.myDevices[deviceManager.currentDeviceIndex];
  int outputNumber = 0;
  for (size_t i = 0; i <= relayIndex; i++) {
    if (i < currentDevice.relays.size() && currentDevice.relays[i].isOutput) {
      outputNumber++;
    }
  }
  return outputNumber;
}

void TelegramBot::handleRelayCommand(int64_t chatId, const String& command) {
  TBMessage msg;
  msg.chatId = chatId;
  if (!hasPermission(String(chatId), "writing")) {
    myBot.sendMessage(msg, "❌ У вас нет прав для выполнения этой команды.");
    return;
  }

  String action = "";
  int relayIndex = -1;
  int relayNumber = -1;

  if (command.startsWith("/on") || command.equalsIgnoreCase("on")) {
    action = "on";
    String numStr = command.startsWith("/on") ? command.substring(3) : command.substring(2);
    relayNumber = numStr.toInt();
  } else if (command.startsWith("/off") || command.equalsIgnoreCase("off")) {
    action = "off";
    String numStr = command.startsWith("/off") ? command.substring(4) : command.substring(3);
    relayNumber = numStr.toInt();
  } else if (command == "/resetmanual" || command.equalsIgnoreCase("resetmanual")) {
    action = "reset_all";
  }

  if (action != "reset_all") {
    if (relayNumber <= 0) {
      myBot.sendMessage(msg, "❌ Неверный формат команды. Используйте /on1, /off2 и т.д.");
      return;
    }
    const Device& currentDevice = deviceManager.myDevices[deviceManager.currentDeviceIndex];
    int currentOutputNumber = 0;
    bool relayFound = false;
    for (size_t i = 0; i < currentDevice.relays.size(); ++i) {
      if (currentDevice.relays[i].isOutput) {
        currentOutputNumber++;
        if (currentOutputNumber == relayNumber) {
          relayIndex = i;
          relayFound = true;
          break;
        }
      }
    }
    if (!relayFound || relayIndex < 0) {
      myBot.sendMessage(msg, "❌ Реле с номером " + String(relayNumber) + " не существует.");
      return;
    }
  }

  StaticJsonDocument<JSON_BUFFER_SIZE> doc;
  if (action == "reset_all") {
    doc["action"] = "reset_all";
  } else {
    const Device& currentDevice = deviceManager.myDevices[deviceManager.currentDeviceIndex];
    doc["relay"] = currentDevice.relays[relayIndex].id;
    doc["action"] = action;
  }

  if (deviceManager.handleRelayCommand(doc.as<JsonObject>(), 0)) {
    String successMsg = "✅ Команда выполнена: ";
    if (action == "reset_all") {
      successMsg += "Все реле сброшены в автоматический режим";
    } else {
      const Device& currentDevice = deviceManager.myDevices[deviceManager.currentDeviceIndex];
      String relayName = currentDevice.relays[relayIndex].description;
      successMsg += String(action == "on" ? "Включено" : "Выключено") + " реле " + String(relayNumber) + " (" + relayName + ")";
    }
    myBot.sendMessage(msg, successMsg.c_str());
    sendSimpleStatus(chatId);
  } else {
    myBot.sendMessage(msg, "❌ Не удалось выполнить команду: " + command);
  }
}

void TelegramBot::handleSystemToggleCommand(int64_t chatId, const String& command) {
  if (!hasPermission(String(chatId), "writing")) {
    TBMessage msg;
    msg.chatId = chatId;
    myBot.sendMessage(msg, "❌ У вас нет прав для выполнения этой команды.");
    return;
  }
  if (deviceManager.myDevices.empty() || deviceManager.currentDeviceIndex >= deviceManager.myDevices.size()) {
    return;
  }

  Device& device = deviceManager.myDevices[deviceManager.currentDeviceIndex];
  bool stateChanged = false;

  if (command == "/timers_on") {
    if (!device.isTimersEnabled) {
      device.isTimersEnabled = true;
      stateChanged = true;
    }
  }
  else if (command == "/timers_off") {
    if (device.isTimersEnabled) {
      device.isTimersEnabled = false;
      stateChanged = true;
    }
  }
  else if (command == "/schedule_on") {
    if (!device.isScheduleEnabled) {
      device.isScheduleEnabled = true;
      stateChanged = true;
    }
  }
  else if (command == "/schedule_off") {
    if (device.isScheduleEnabled) {
      device.isScheduleEnabled = false;
      stateChanged = true;
    }
  }
  else if (command == "/temp_on") {
    if (!device.temperature.isUseSetting) {
      device.temperature.isUseSetting = true;
      stateChanged = true;
    }
  }
  else if (command == "/temp_off") {
    if (device.temperature.isUseSetting) {
      device.temperature.isUseSetting = false;
      stateChanged = true;
    }
  }
  else if (command == "/sensors_on") {
    if (!device.isActionEnabled) {
      device.isActionEnabled = true;
      stateChanged = true;
    }
  }
  else if (command == "/sensors_off") {
    if (device.isActionEnabled) {
      device.isActionEnabled = false;
      stateChanged = true;
    }
  }
  else if (command == "/push_error_on") {
    if (!settings.ws.telegramSettings.isPush[0]) {
      settings.ws.telegramSettings.isPush[0] = true;
      stateChanged = true;
    }
  }
  else if (command == "/push_error_off") {
    if (settings.ws.telegramSettings.isPush[0]) {
      settings.ws.telegramSettings.isPush[0] = false;
      stateChanged = true;
    }
  }
  else if (command == "/push_info_on") {
    if (!settings.ws.telegramSettings.isPush[1]) {
      settings.ws.telegramSettings.isPush[1] = true;
      stateChanged = true;
    }
  }
  else if (command == "/push_info_off") {
    if (settings.ws.telegramSettings.isPush[1]) {
      settings.ws.telegramSettings.isPush[1] = false;
      stateChanged = true;
    }
  }
  else if (command == "/push_user_on") {
    if (!settings.ws.telegramSettings.isPush[2]) {
      settings.ws.telegramSettings.isPush[2] = true;
      stateChanged = true;
    }
  }
  else if (command == "/push_user_off") {
    if (settings.ws.telegramSettings.isPush[2]) {
      settings.ws.telegramSettings.isPush[2] = false;
      stateChanged = true;
    }
  }

  if (stateChanged) {
    settings.saveSettings();
  }
  sendSimpleStatus(chatId);
}

void TelegramBot::loop() {
  yield();
  if (!settings.ws.telegramSettings.isTelegramOn) {
    if (isBegin) {
      Serial.println("🛑 Telegram Stop requested by user.");
      client.stop();
      isBegin = false;
      isColdStart = true;
    }
    return;
  }
  if (!isBegin) {
    unsigned long now = millis();
    if (now - lastReconnectAttempt > RECONNECT_INTERVAL) {
      lastReconnectAttempt = now;
      begin();
    }
    return;
  }
  if (!client.connected()) {
    isBegin = false;
    client.stop();
    return;
  }
  if (webServer.isBusy()) return;
  if (doRestart) {
    doRestartProcedure();
    return;
  }
  checkAndSendLogs();

  if (!isDownloading && isBegin) {
    TBMessage msg;
    if (myBot.getNewMessage(msg)) {
      String userId = String(msg.sender.id);
      if (!hasPermission(userId, "reading")) {
        myBot.sendMessage(msg, "❌ Доступ запрещен.");
        return;
      }
      if (msg.messageType == MessageText) {
        String text = msg.text; text.trim();
        if (text == "/start" || text == "/start start" || text == "start") {
          Serial.println("Sending simple status...");
          sendSimpleStatus(msg.sender.id);
        }
        else if (text == "/help" || text == "help") {
          Serial.println("Sending help...");
          sendHelpMessage(msg.sender.id);
        }
        else if (text == "/reset" || text == "reset") {
          if (hasPermission(userId, "writing")) {
            myBot.sendMessage(msg, "🔄 Перезагрузка устройства...");
            doRestart = true;
          }
          else {
            myBot.sendMessage(msg, "❌ Нет прав на перезагрузку.");
          }
        }
        else if (text == "/update" || text == "update") {
          myBot.sendMessage(msg, "📲 Отправьте файл прошивки (.bin) для обновления.");
        }
        else if (text.startsWith("/newtoken")) {
          if (hasPermission(userId, "writing")) {
            String newToken = text.substring(10);
            newToken.trim();
            if (isValidTokenFormat(newToken)) {
              settings.ws.telegramSettings.botId = newToken;
              settings.saveSettings();
              myBot.sendMessage(msg, "✅ Новый токен сохранен. Бот будет перезапущен.");
              isBegin = false;
              begin();
            } else {
              myBot.sendMessage(msg, "❌ Неверный формат токена.");
            }
          } else {
            myBot.sendMessage(msg, "❌ Нет прав на смену токена.");
          }
        }
        else if (text == "/resetmanual" || text == "resetmanual") {
          handleRelayCommand(msg.sender.id, text);
        }
        else if (text.startsWith("/on") || text.startsWith("/off")) {
          handleRelayCommand(msg.sender.id, text);
        }
        else if (text.startsWith("/timers_") || text.startsWith("/schedule_") ||
                 text.startsWith("/temp_") || text.startsWith("/sensors_") ||
                 text.startsWith("/push_error_") || text.startsWith("/push_info_") ||
                 text.startsWith("/push_user_")) {
          handleSystemToggleCommand(msg.sender.id, text);
        }
        else if (text.indexOf("/get") > -1) {
          String command = text;
          int spaceIndex = command.indexOf(' ');
          if (spaceIndex > -1) {
            String filename = command.substring(spaceIndex + 1);
            filename.trim();
            String fullPath = "/" + filename;
            if (SPIFFS.exists(fullPath)) {
              Serial.println("Sending file: " + fullPath);
              sendDocument(msg, AsyncTelegram2::DocumentType::TEXT, fullPath.c_str(), "This is caption");
            } else {
              Serial.println("File not found: " + fullPath);
              myBot.sendMessage(msg, "❌ File not found: " + filename);
            }
          } else {
            myBot.sendMessage(msg, "📁 Please specify a filename: /get filename.txt");
          }
        }
        else {
          myBot.sendMessage(msg, "❓ Неизвестная команда. Используйте /help для списка команд.");
        }
      }
      else if (msg.messageType == MessageQuery) {
        String callbackData = msg.callbackQueryData;
        Serial.printf("🔄 Callback query: %s\n", callbackData.c_str());

        if (callbackData == "help") {
          sendHelpMessage(msg.sender.id);
        }
        else if (callbackData == "reset") {
          if (hasPermission(userId, "writing")) {
            myBot.endQuery(msg, "Перезагрузка инициирована...", false);
            doRestart = true;
          } else {
            myBot.endQuery(msg, "❌ У вас нет прав на перезагрузку.", true);
          }
        }
        else if (callbackData == "update") {
          myBot.sendMessage(msg, "📲 Отправьте файл прошивки (.bin) для обновления.");
        }
        else if (callbackData == "newtoken") {
          myBot.sendMessage(msg, "🔑 Отправьте команду в виде: /newtoken ВАШ_НОВЫЙ_ТОКЕН");
        }
        else if (callbackData == "resetmanual") {
          handleRelayCommand(msg.sender.id, "resetmanual");
        }
        else if (callbackData.startsWith("on") || callbackData.startsWith("off")) {
          handleRelayCommand(msg.sender.id, callbackData);
        }
        else if (callbackData.equalsIgnoreCase(CONFIRM)) {
          myBot.endQuery(msg, "Start flashing... please wait (~30/60s)", true);
          shouldStartDownload = true;
        }
        else if (callbackData.equalsIgnoreCase(CANCEL)) {
          myBot.endQuery(msg, "Flashing canceled", true);
        }
      }
      else if (msg.messageType == MessageDocument) {
        document = msg.document.file_path;
        fileName = msg.document.file_name;

        if (msg.document.file_exists) {
          String report = "📲 **Обновление прошивки**\n\n";
          report += "📄 Файл: " + String(msg.document.file_name) + "\n";
          report += "📦 Размер: " + String(msg.document.file_size / 1024) + " KB\n\n";
          report += "Начать обновление?";

          InlineKeyboard confirmKbd;
          confirmKbd.addButton("✅ FLASH", CONFIRM, KeyboardButtonQuery);
          confirmKbd.addButton("❌ CANCEL", CANCEL, KeyboardButtonQuery);
          myBot.sendMessage(msg, report.c_str(), confirmKbd);
        } else {
          myBot.sendMessage(msg, "❌ Файл недоступен. Возможно превышен лимит 20MB или файл удален.");
        }
      }
    }

  }

  if (shouldStartDownload && !isDownloading) {
    ota.dlState.cleanup();
    isDownloading = true;
    webServer.stopServer();
  }

  if (isDownloading) {
    Ota::ProcessStatus status = ota.handleUpdate(document, fileName, true);

    switch (status) {
      case Ota::HTTP_UPDATE_OK:
      case Ota::HTTP_FILE_END:
        Serial.println("✅ Download completed successfully!");
        shouldStartDownload = false;
        isDownloading = false;
        checkMemory();
        webServer.startServer();
        break;

      case Ota::HTTP_UPDATE_FAILED:
      case Ota::HTTP_FILE_FAILED:
        Serial.println("❌ Download failed!");
        shouldStartDownload = false;
        isDownloading = false;
        checkMemory();
        webServer.startServer();
        break;

      case Ota::HTTP_DOWNLOAD_IN_PROGRESS:
        break;

      default:
        break;
    }
  }
  yield();
}
bool TelegramBot::isValidTokenFormat(const String& token) {
  int colonIndex = token.indexOf(':');
  if (colonIndex == -1 || colonIndex == 0 || colonIndex == token.length() - 1) return false;
  String userIdPart = token.substring(0, colonIndex);
  for (int i = 0; i < userIdPart.length(); i++) if (!isDigit(userIdPart[i])) return false;
  String secretPart = token.substring(colonIndex + 1);
  if (secretPart.length() < 20) return false;
  return true;
}

void TelegramBot::checkMemory() {
#ifdef ESP8266
  Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
  Serial.printf("Free sketch space: %d bytes\n", ESP.getFreeSketchSpace());
  Serial.printf("Heap fragmentation: %d%%\n", ESP.getHeapFragmentation());
#elif defined(ESP32)
  Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
  Serial.printf("Free sketch space: %d bytes\n", ESP.getFreeSketchSpace());
#endif
}

void TelegramBot::sendDocument(TBMessage &msg, AsyncTelegram2::DocumentType fileType, const char* filename, const char* caption) {
  Serial.print("\nFilename: "); Serial.println(filename);

#ifdef ESP32
  if (!SPIFFS.exists("/") && !SPIFFS.begin(true)) {
#elif defined(ESP8266)
  if (!SPIFFS.exists("/") && !SPIFFS.begin()) {
#endif
    Serial.println("Failed to mount SPIFFS");
    return;
  }

  File file = SPIFFS.open(filename, "r");
  if (file) {
    myBot.sendDocument(msg, file, file.size(), fileType, file.name(), caption);
    myBot.sendMessage(msg, "✅ Log file sent");
    file.close();
  } else {
    Serial.println("❌ Can't open file. Upload \"data\" folder to filesystem");
    return;
  }
}

void TelegramBot::doRestartProcedure() {
  doRestart = false;
  Serial.println("🔄 Starting restart procedure...");

  if (millis() - startupTime < 10000) {
    Serial.println("⚠️ Игнорируем перезагрузку, устройство только что запустилось.");
    return;
  }

  Serial.println("✅ Перезагрузка разрешена. Перезагружаем через 1 секунду...");
  delay(1000);
  Serial.println("🚀 Restarting ESP...");
  ESP.restart();
}

void TelegramBot::checkAndSendLogs() {
  if (!isBegin || !settings.ws.telegramSettings.isTelegramOn) {
    return;
  }

  static unsigned long lastLogCheckTime = 0;
  if (millis() - lastLogCheckTime < LOG_CHECK_INTERVAL) {
    return;
  }
  lastLogCheckTime = millis();

  _unsentLogsBuffer.clear();
  uint8_t unsentCount = logger.getUnsentMessages(_unsentLogsBuffer, 3);

  if (unsentCount == 0) {
    return;
  }

#ifdef LOGGER_DEBUG
  Serial.printf("[TELEGRAM] Found %d unsent logs to process.\n", unsentCount);
#endif

  uint8_t sentThisCycle = 0;
  const uint8_t MAX_SENT_PER_CYCLE = 2;

  for (auto& log_pair : _unsentLogsBuffer) {
    if (sentThisCycle >= MAX_SENT_PER_CYCLE) {
      break;
    }

    LogEntry* logEntry = log_pair.first;
    uint8_t logIndex = log_pair.second;

    bool logWasSent = false;

    if (shouldSendLog(*logEntry)) {

      for (const auto& user : settings.ws.telegramSettings.telegramUsers) {
        if (user.reading && shouldSendLogToUser(user.id.toInt())) {

          if (sendLogMessage(*logEntry, user.id.toInt())) {
            logWasSent = true;
          }
          yield();
        }
      }
    }

    if (logWasSent || !shouldSendLog(*logEntry)) {
      logger.markAsSent(logIndex);
      if (logWasSent) {
        sentThisCycle++;
      }
#ifdef LOGGER_DEBUG
      Serial.printf("[TELEGRAM] Log (type %d) processed. Marked as sent to clear queue.\n", logEntry->typeMsg);
#endif
    }
  }

  if (logger.getUnsentCount() == 0 && sentThisCycle > 0) {
    Serial.println("[TELEGRAM] All pending logs have been sent. Saving final batch to SPIFFS.");
    logger.forceSave();
  }
}

bool TelegramBot::sendLogMessage(const LogEntry & logEntry, int64_t chatId) {
  TBMessage msg;
  msg.chatId = chatId;

  char formattedMessage[512];

  const char* typePrefix;
  switch (logEntry.typeMsg) {
    case LOG_ERROR: typePrefix = "🚨 ОШИБКА: "; break;
    case LOG_INFO:  typePrefix = "ℹ️ ИНФО: "; break;
    case LOG_USER:  typePrefix = "👤 ПОЛЬЗОВАТЕЛЬ: "; break;
    default:        typePrefix = "❓ НЕИЗВЕСТНЫЙ ТИП: "; break;
  }

  snprintf(formattedMessage, sizeof(formattedMessage),
           "[%s] %s%s",
           logEntry.timestamp,
           typePrefix,
           logEntry.message);

  bool success = myBot.sendMessage(msg, formattedMessage, nullptr, true);

#ifdef LOGGER_DEBUG
  if (success) {
    Serial.printf("[TELEGRAM] Log sent to %lld: type=%d, msg='%s'\n",
                  chatId, logEntry.typeMsg, logEntry.message);
  }
#endif

  return success;
}

bool TelegramBot::shouldSendLog(const LogEntry & logEntry) {

  if (logEntry.isSay) {
    return false;
  }

  const uint8_t numPushTypes = sizeof(settings.ws.telegramSettings.isPush) / sizeof(settings.ws.telegramSettings.isPush[0]);

  if (logEntry.typeMsg >= numPushTypes) {
    return false;
  }

#ifdef LOGGER_DEBUG
  Serial.printf("[TELEGRAM] Checking log type %d, isPush[%d] = %s\n",
                logEntry.typeMsg,
                logEntry.typeMsg,
                settings.ws.telegramSettings.isPush[logEntry.typeMsg] ? "true" : "false");
#endif

  return settings.ws.telegramSettings.isPush[logEntry.typeMsg];
}

bool TelegramBot::shouldSendLogToUser(int64_t chatId) {

  String chatIdStr = String(chatId);

  for (const auto& user : settings.ws.telegramSettings.telegramUsers) {

    if (user.id == chatIdStr) {

      return user.reading;
    }
  }

  return false;
}

void TelegramBot::updateBotCommandsOnServer(bool start) {

  static enum {
    IDLE,
    DELETING,
    WAITING_AFTER_DELETE,
    SETTING,
    WAITING_BETWEEN_COMMANDS,
    COMPLETE
  } state = IDLE;

  static unsigned long lastActionTime = 0;
  static int currentCommandIndex = 0;

  struct BotCommand {
    const char* command;
    const char* description;
  };
  BotCommand commands[] = {
    {"start", "Запустить бота"},
    {"status", "Текущий статус устройства"},
    {"help", "Справка по командам"},
    {"reset", "Перезагрузить устройство"},
    {"update", "Обновить прошивку"}
  };
  const int numCommands = sizeof(commands) / sizeof(commands[0]);

  if (start) {
    state = DELETING;
    lastActionTime = millis();
    currentCommandIndex = 0;
    logger.addLog("[TELEGRAM-CMD] Запуск процесса обновления команд...");
    return;
  }

  if (state == IDLE || state == COMPLETE) {
    return;
  }

  unsigned long currentMillis = millis();

  switch (state) {
    case DELETING:
      logger.addLog("[TELEGRAM-CMD] Шаг 1: Очистка старых команд...");
      if (!myBot.deleteMyCommands()) {
        logger.addLog("[TELEGRAM-CMD] ⚠️ Не удалось очистить старые команды. Процесс остановлен.");
        state = COMPLETE;
      } else {
        logger.addLog("[TELEGRAM-CMD] ✅ Старые команды очищены.");
        lastActionTime = currentMillis;
        state = WAITING_AFTER_DELETE;
      }
      break;

    case WAITING_AFTER_DELETE:
      if (currentMillis - lastActionTime >= 1000) {
        Serial.println("[TELEGRAM-CMD] Шаг 2: Установка новых команд...");
        state = SETTING;
      }
      break;

    case SETTING:
      if (currentCommandIndex < numCommands) {
        const BotCommand& cmd = commands[currentCommandIndex];
        Serial.printf("[TELEGRAM-CMD] Установка команды /%s...\n", cmd.command);
        if (!myBot.setMyCommands(cmd.command, cmd.description)) {
          Serial.printf("[TELEGRAM-CMD] ❌ Не удалось установить команду /%s\n", cmd.command);
        } else {
          Serial.printf("[TELEGRAM-CMD] ✅ Команда /%s установлена.\n", cmd.command);
        }
        lastActionTime = currentMillis;
        state = WAITING_BETWEEN_COMMANDS;
      } else {
        logger.addLog("[TELEGRAM-CMD] ✅ Все новые команды успешно установлены!");
        state = COMPLETE;
      }
      break;

    case WAITING_BETWEEN_COMMANDS:
      if (currentMillis - lastActionTime >= 1000) {
        currentCommandIndex++;
        state = SETTING;
      }
      break;

    case COMPLETE:

      break;
  }
}
