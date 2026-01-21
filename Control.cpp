#include "Control.h"

 Control::Control(DeviceManager& dm, Logger& logger)
    : deviceManager(dm),
      logger(logger),
      myDevices(dm.myDevices),
      currentDeviceIndex(dm.currentDeviceIndex)
{

    myPID = nullptr;
    lastPidTime = 0;
    pidWindowStartTime = 0;
    debug = false;
    lastUpdate = 0;

    touchStates.clear();
  }

  void Control::setup() {
    setupControl();
    logger.addLog("Control setup completed");
  }

 void Control::loop() {
    unsigned long now = millis();

      readSensors();

    static unsigned long lastSensorRead = 0;
    if (now - lastSensorRead >= 500) {

        lastSensorRead = now;
    }

    static unsigned long lastPinUpdate = 0;
    if (now - lastPinUpdate >= 250) {
        if ((now - lastSensorRead) > 100) {
            updatePins();
            lastPinUpdate = now;
        }
    }

    static unsigned long lastScheduleUpdate = 0;
    static unsigned long schedulePhase = 0;
    if (now - lastScheduleUpdate >= 1000) {
        setSchedules();
        lastScheduleUpdate = now;
        schedulePhase = now % 3;
    }

    static unsigned long lastTimerUpdate = 0;
    if (now - lastTimerUpdate >= 1000) {

        if ((now - lastScheduleUpdate) > 250) {
            setTimersExecute();
            lastTimerUpdate = now;
        }
    }

    static unsigned long lastTempControl = 0;
    if (now - lastTempControl >= 2000) {

        if ((now - lastTimerUpdate) > 500) {
            setTemperature();
            lastTempControl = now;
        }
    }

    static unsigned long lastSensorAction = 0;
    if (now - lastSensorAction >= 1000) {

        if ((now % 1000) > 750) {
            setSensorActions();
            lastSensorAction = now;
        }
    }
}

  bool Control::isNumeric(const String& str) {
    for (size_t i = 0; i < str.length(); i++) {
      if (!isdigit(str.charAt(i))) {
        return false;
      }
    }
    return true;
  }

  bool Control::isValidDateTime(const String& dateTime) {

    int spaceIndex = dateTime.indexOf(' ');
    if (spaceIndex == -1) return false;

    String date = dateTime.substring(0, spaceIndex);
    String time = dateTime.substring(spaceIndex + 1);

    int dot1 = date.indexOf('.');
    int dot2 = date.lastIndexOf('.');

    if (dot1 == -1 || dot2 == -1 || dot1 == dot2) return false;

    String day = date.substring(0, dot1);
    String month = date.substring(dot1 + 1, dot2);
    String year = date.substring(dot2 + 1);

    if (!isNumeric(day) || !isNumeric(month) || !isNumeric(year)) return false;

    int dayInt = day.toInt();
    int monthInt = month.toInt();
    int yearInt = year.toInt();

    if (dayInt < 1 || dayInt > 31 || monthInt < 1 || monthInt > 12 || yearInt < 1000 || yearInt > 9999) {
      return false;
    }

    int colonIndex = time.indexOf(':');
    if (colonIndex == -1) return false;

    String hour = time.substring(0, colonIndex);
    String minute = time.substring(colonIndex + 1);

    if (!isNumeric(hour) || !isNumeric(minute)) return false;

    int hourInt = hour.toInt();
    int minuteInt = minute.toInt();

    if (hourInt < 0 || hourInt > 23 || minuteInt < 0 || minuteInt > 59) {
      return false;
    }

    return true;
  }

  int Control::shiftWeekDay(int currentDay) {
    if (currentDay == 0) {
      return 6;
    } else {
      return currentDay - 1;
    }
  }

  time_t Control::convertToTimeT(const String& date) {
    tm t = {};
    sscanf(date.c_str(), "%4d-%2d-%2d", &t.tm_year, &t.tm_mon, &t.tm_mday);

    t.tm_year -= 1900;
    t.tm_mon -= 1;

    t.tm_hour = 0;
    t.tm_min = 0;
    t.tm_sec = 0;

    return mktime(&t);
  }

  time_t Control::getCurrentTime() {

    return time(nullptr);
  }

  String Control::formatDateTime(time_t rawTime) {
    struct tm timeInfo;
    localtime_r(&rawTime, &timeInfo);

    char buffer[20];
    snprintf(buffer, sizeof(buffer), "%02d.%02d.%04d %02d:%02d:%02d",
             timeInfo.tm_mday,
             timeInfo.tm_mon + 1,
             timeInfo.tm_year + 1900,
             timeInfo.tm_hour,
             timeInfo.tm_min,
             timeInfo.tm_sec);

    return String(buffer);
  }

  time_t Control::convertOnlyDate(time_t rawTime) {
    struct tm timeInfo;
    localtime_r(&rawTime, &timeInfo);

    timeInfo.tm_hour = 0;
    timeInfo.tm_min = 0;
    timeInfo.tm_sec = 0;

    return mktime(&timeInfo);
  }

  uint32_t Control::timeStringToSeconds(const String& timeStr) {
    int hours, minutes, seconds;
    if (sscanf(timeStr.c_str(), "%d:%d:%d", &hours, &minutes, &seconds) == 3) {
      return hours * 3600 + minutes * 60 + seconds;
    }
    return 0;
  }

  String Control::secondsToTimeString(uint32_t totalSeconds) {
    int hours = totalSeconds / 3600;
    int minutes = (totalSeconds % 3600) / 60;
    int seconds = totalSeconds % 60;
    char buffer[9];
    snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", hours, minutes, seconds);
    return String(buffer);
  }

  void Control::updatePins() {
    if (myDevices.empty()) return;

    Device& currentDevice = myDevices[currentDeviceIndex];
    static std::unordered_map<int, bool> lastPinStates;
    static std::unordered_map<int, uint8_t> lastPwmValues;

    for (auto& relay : currentDevice.relays) {

      if (relay.isOutput) {

        if (relay.isPwm) {
          uint8_t newPwmValue = relay.pwm;

          if (lastPwmValues[relay.pin] != newPwmValue) {
            analogWrite(relay.pin, newPwmValue);
            lastPwmValues[relay.pin] = newPwmValue;

            char logBuffer[64];
            snprintf(logBuffer, sizeof(logBuffer), "PWM обновлено | PIN: %d -> %d",
                     relay.pin, newPwmValue);
            logger.addLog(logBuffer);
          }

        }

        else {
          bool newState = relay.statePin;

          if (lastPinStates[relay.pin] != newState) {
            digitalWrite(relay.pin, newState ? HIGH : LOW);
            lastPinStates[relay.pin] = newState;

            char logBuffer[64];
            snprintf(logBuffer, sizeof(logBuffer), "Реле обновлено | PIN: %d -> %s",
                     relay.pin, newState ? "HIGH" : "LOW");
            logger.addLog(logBuffer);
          }
        }

      }
    }
}

  void Control::controlOutputs(OutPower& outPower) {
    if (!outPower.isUseSetting) return;

    if (myDevices.empty()) return;
    Device& device = myDevices[currentDeviceIndex];

    Relay* relay = findRelayById(device, outPower.relayId);

    if (relay) {

      if (relay->isOutput && !relay->manualMode) {
        relay->statePin = outPower.statePin;
      }
    }
  }

void Control::setTemperature() {

  if (myDevices.empty()) return;

  Device& device = myDevices[currentDeviceIndex];
  Temperature& temp = device.temperature;

  static bool wasActive = false;
  static uint8_t lastDeviceIndex = 255;
  static int lastSensorId = -1;
  static int lastRelayId = -1;

  if (lastDeviceIndex != currentDeviceIndex) {
    lastDeviceIndex = currentDeviceIndex;
    wasActive = false;
    lastSensorId = -1;
    lastRelayId = -1;
    temp.sensorPtr = nullptr;
    temp.relayPtr = nullptr;
  }

  if (!temp.isUseSetting) {
    if (wasActive) {

      if (temp.relayPtr) {
        temp.relayPtr->statePin = temp.relayPtr->lastState;
        temp.relayPtr->isPwm = false;
        temp.relayPtr->pwm = 0;
      }

      if (myPID) {
        myPID.reset();
        myPID = nullptr;
      }

      wasActive = false;
      logger.addLog("Температурный контроль деактивирован");
    }
    return;
  }

  if (!wasActive) {

    temp.sensorPtr = findSensorById(device, temp.sensorId);
    temp.relayPtr = findRelayById(device, temp.relayId);

    if (!temp.sensorPtr || !temp.relayPtr) {
      logger.addLog("Ошибка температурного контроля: не найден сенсор или реле", 0);
      temp.isUseSetting = false;
      return;
    }

    temp.relayPtr->lastState = temp.relayPtr->statePin;

    if (temp.collectionSettings.get(0) && temp.selectedPidIndex < device.pids.size()) {
      Pid& pidSettings = device.pids[temp.selectedPidIndex];

      inputPid = static_cast<double>(temp.sensorPtr->currentValue);
      setpointPid = static_cast<double>(temp.setTemperature);
      outputPid = 0.0;

      myPID = std::make_unique<PID>(
        &inputPid, &outputPid, &setpointPid,
        pidSettings.Kp, pidSettings.Ki, pidSettings.Kd,
        temp.isIncrease ? DIRECT : REVERSE
      );

      if (myPID) {
        myPID->SetMode(AUTOMATIC);
        myPID->SetOutputLimits(0, pidWindowSize);
        myPID->SetSampleTime(1000);
      }

      pidWindowStartTime = millis();
    }

    wasActive = true;
    logger.addLog("Температурный контроль активирован");
  }

  if (!temp.sensorPtr || !temp.relayPtr) {
    return;
  }

  temp.currentTemp = temp.sensorPtr->currentValue;

  if (temp.sensorId != lastSensorId || temp.relayId != lastRelayId) {
    lastSensorId = temp.sensorId;
    lastRelayId = temp.relayId;
    wasActive = false;
    return;
  }

  if (temp.isSmoothly && myPID) {
    inputPid = static_cast<double>(temp.currentTemp);
    setpointPid = static_cast<double>(temp.setTemperature);

    myPID->Compute();
    temp.pidOutputMs = static_cast<unsigned long>(outputPid);

    temp.relayPtr->isPwm = true;
    uint8_t pwmValue = map(static_cast<long>(outputPid), 0, pidWindowSize, 0, 255);
    if (!temp.relayPtr->manualMode) {
      temp.relayPtr->pwm = pwmValue;
    }

    static unsigned long lastPidLog = 0;
    if (millis() - lastPidLog > 30000) {
      char logBuffer[100];
      snprintf(logBuffer, sizeof(logBuffer),
               "PID: Temp=%.1f°C, Set=%d°C, PWM=%d%%",
               temp.currentTemp, temp.setTemperature, (pwmValue * 100) / 255);
      logger.addLog(logBuffer);
      lastPidLog = millis();
    }
  }

  else {
    temp.relayPtr->isPwm = false;

    if (temp.isIncrease) {

      if (temp.collectionSettings.get(0) && !temp.relayPtr->manualMode) { temp.relayPtr->statePin = (temp.currentTemp < temp.setTemperature); }
      else if (temp.collectionSettings.get(1) ) {  device.isTimersEnabled = (temp.pidOutputMs > 0); }
    } else {

       if (temp.collectionSettings.get(0) && !temp.relayPtr->manualMode) {  temp.relayPtr->statePin = (temp.currentTemp > temp.setTemperature); }
         else if (temp.collectionSettings.get(1) ) {  device.isTimersEnabled = (temp.pidOutputMs < pidWindowSize * 0.5); }
    }
  }
}

Relay* Control::findRelayById(Device& device, int id) {

  for (auto& relay : device.relays) {
    if (relay.id == id) {
      return &relay;
    }
  }
  return nullptr;
}

Sensor* Control::findSensorById(Device& device, int id) {
  for (auto& sensor : device.sensors) {
    if (sensor.sensorId == id) {
      return &sensor;
    }
  }
  return nullptr;
}

  void Control::setFlagsSettingsTimers(uint8_t selectedIndex, Timer& currentTimer) {

    if (selectedIndex != 1) {

      currentTimer.endStateRelay.isUseSetting = false;
    }
  }

  void Control::collectionSettingsTimer(uint8_t currentTimerIndex) {
    static bool prevHadTempControl = false;

    if (myDevices.empty()) return;
    Device& device = myDevices[currentDeviceIndex];
    Timer& currentTimer = device.timers[currentTimerIndex];

    if (prevHadTempControl && !currentTimer.collectionSettings.get(0)) {

      device.temperature.isUseSetting = false;
    }
    else if (currentTimer.collectionSettings.get(0)) {

      device.temperature.isUseSetting = true;
    }

    prevHadTempControl = currentTimer.collectionSettings.get(0);

    if (currentTimer.collectionSettings.get(1)) {
      controlOutputs(currentTimer.endStateRelay);
      setFlagsSettingsTimers(1, currentTimer);
    }
  }

  void Control::executeTimers(bool state) {

    static size_t currentTimerIndex = 0;
    static unsigned long timerStartTime = 0;
    static bool timersCompleted = false;

    if (myDevices.empty()) return;
    Device& device = myDevices[currentDeviceIndex];

    if (!device.isTimersEnabled || device.timers.empty()) {

      for (auto& timer : device.timers) {
        timer.progress = {0, 0, false, false};
      }

      currentTimerIndex = 0;
      timersCompleted = false;
      return;
    }

    if (timersCompleted && !device.isEncyclateTimers) {
      return;
    }

    if (!device.timers[currentTimerIndex].progress.isRunning) {

      while (currentTimerIndex < device.timers.size() && !device.timers[currentTimerIndex].isUseSetting) {
        currentTimerIndex++;
      }

      if (currentTimerIndex >= device.timers.size()) {
        return;
      }

      device.timers[currentTimerIndex].progress.isStopped = false;
      timerStartTime = millis();
      device.timers[currentTimerIndex].progress.isRunning = true;
      controlOutputs(device.timers[currentTimerIndex].initialStateRelay);
      timersCompleted = false;
    }

    if (device.timers[currentTimerIndex].progress.isRunning) {

      if (!device.timers[currentTimerIndex].isUseSetting) {
        Serial.printf("Таймер %d был отключен пользователем во время работы. Останавливаю.\n", currentTimerIndex);

        device.timers[currentTimerIndex].progress.isRunning = false;
        device.timers[currentTimerIndex].progress.isStopped = true;

        currentTimerIndex = 0;
        timersCompleted = false;

      } else {

        uint32_t timerDuration = timeStringToSeconds(device.timers[currentTimerIndex].time);
        unsigned long currentTime = millis();
        unsigned long elapsedTime = (currentTime - timerStartTime) / 1000;
        unsigned long remainingTime = (timerDuration > elapsedTime) ? (timerDuration - elapsedTime) : 0;

        device.timers[currentTimerIndex].progress = {
          elapsedTime,
          remainingTime,
          true,
          false,
        };

        if (elapsedTime >= timerDuration) {
          collectionSettingsTimer(currentTimerIndex);
          device.timers[currentTimerIndex].progress.isRunning = false;
          device.timers[currentTimerIndex].progress.isStopped = true;

          size_t nextTimerIndex = currentTimerIndex + 1;
          while (nextTimerIndex < device.timers.size() && !device.timers[nextTimerIndex].isUseSetting) {
            nextTimerIndex++;
          }

          if (nextTimerIndex >= device.timers.size()) {
            if (!device.isEncyclateTimers) {
              timersCompleted = true;
              return;
            }

            nextTimerIndex = 0;
            while (nextTimerIndex < device.timers.size() && !device.timers[nextTimerIndex].isUseSetting) {
              nextTimerIndex++;
            }
            if (nextTimerIndex >= device.timers.size()) return;
          }

          currentTimerIndex = nextTimerIndex;
          timerStartTime = millis();
          controlOutputs(device.timers[currentTimerIndex].initialStateRelay);

        }
      }
    }
  }

  void Control::setTimersExecute() {
    static bool isInitialStateSaved = false;
    static bool isEndStateApplied = false;
    static bool lastStateTemperature = false;
    static bool prevTimersEnabled = false;

    if (myDevices.empty()) return;
    Device& device = myDevices[currentDeviceIndex];
    bool stateChanged = (device.isTimersEnabled != prevTimersEnabled);
    prevTimersEnabled = device.isTimersEnabled;

    if (stateChanged && device.isTimersEnabled) {
      lastStateTemperature = device.temperature.isUseSetting;
      logger.addLog("Сохраняем таймеры");

      std::unordered_set<uint8_t> relayIds;
      for (auto& timer : device.timers) {
        if (timer.isUseSetting) {
          relayIds.insert(timer.initialStateRelay.relayId);
          relayIds.insert(timer.endStateRelay.relayId);
        }
        timer.progress.clear();
      }

      for (uint8_t relayId : relayIds) {
        Relay* relay = findRelayById(device, relayId);
        if (relay && relay->isOutput) {
          relay->lastState = relay->statePin;
        }
      }

      isInitialStateSaved = true;
      isEndStateApplied = false;
    }

    if (stateChanged && !device.isTimersEnabled && isInitialStateSaved) {
      device.temperature.isUseSetting = lastStateTemperature;
      logger.addLog("Восстанавливаем таймеры");

      std::unordered_set<uint8_t> relayIds;
      for (auto& timer : device.timers) {
        if (timer.isUseSetting) {
          relayIds.insert(timer.initialStateRelay.relayId);
          relayIds.insert(timer.endStateRelay.relayId);
        }
      }

      for (uint8_t relayId : relayIds) {
        Relay* relay = findRelayById(device, relayId);
        if (relay && relay->isOutput) {
          relay->statePin = relay->lastState;
        }
      }

      isEndStateApplied = true;
      isInitialStateSaved = false;
    }

    executeTimers(device.isTimersEnabled);
  }

  void Control::saveRelayStates(uint8_t relayId) {
    if (myDevices.empty()) return;
    Device& device = myDevices[currentDeviceIndex];

    Relay* relay = findRelayById(device, relayId);
    if (relay && relay->isOutput) {
      relay->lastState = relay->statePin;
    }
  }

  void Control::restoreRelayStates(uint8_t relayId) {
    if (myDevices.empty()) return;
    Device& device = myDevices[currentDeviceIndex];

    Relay* relay = findRelayById(device, relayId);
    if (relay && relay->isOutput && !relay->manualMode) {
      relay->statePin = relay->lastState;
    }
  }

  void Control::collectionSettingsSchedule(bool start, ScheduleScenario& scenario) {
    if (myDevices.empty()) return;
    Device& device = myDevices[currentDeviceIndex];

    if (start) {

      scenario.scenarioProcessed = false;

      if (scenario.collectionSettings.get(2)) {
        saveRelayStates(scenario.initialStateRelay.relayId);
      }

      if (scenario.collectionSettings.get(0) && !scenario.temperatureUpdated) {

        device.temperature.isUseSetting = true;
        scenario.temperatureUpdated = true;
        logger.addLog("Температурный контроль активирован (schedule)");
      }

      if (scenario.collectionSettings.get(1) && !scenario.timersExecuted) {

        device.isTimersEnabled = true;
        scenario.timersExecuted = true;
        logger.addLog("Таймеры активированы (schedule)");
      }

      if (scenario.collectionSettings.get(2) && !scenario.initialStateApplied) {

        controlOutputs(scenario.initialStateRelay);
        scenario.initialStateApplied = true;
        logger.addLog("Применено начальное состояние реле (schedule)");
      }

    } else {

      if (!scenario.scenarioProcessed) {
        scenario.scenarioProcessed = true;

        if (scenario.collectionSettings.get(0)) {
          device.temperature.isUseSetting = false;
          restoreRelayStates(device.temperature.relayId);
        }

        if (scenario.collectionSettings.get(1)) {
          device.isTimersEnabled = false;

          for (auto& timer : device.timers) {
            restoreRelayStates(timer.initialStateRelay.relayId);
          }
        }

        if (scenario.collectionSettings.get(2)) {
          restoreRelayStates(scenario.initialStateRelay.relayId);
        }

        if (scenario.endStateRelay.isUseSetting) {
          controlOutputs(scenario.endStateRelay);
          logger.addLog("Применено конечное состояние реле (schedule)");
        }
      }

      scenario.temperatureUpdated = false;
      scenario.timersExecuted = false;
      scenario.initialStateApplied = false;
      scenario.endStateApplied = false;
    }
  }

  void Control::manualWork(String arg) {
    arg.trim();

    if (arg == "?" || arg == "status") {
      logger.addLog(sendStatus());
      return;
    }

    if (arg == "debug") {
      debug = !debug;
      logger.addLog("Debug mode: " + String(debug ? "ON" : "OFF"));
      return;
    }
    logger.addLog("Неизвестная команда: " + arg);
  }

  String Control::getActiveDaysString(const BitArray7& week) {
    String result;
    const char* dayNames[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    for (int i = 0; i < 7; i++) {
      if (week.get(i)) {
        if (result.length() > 0) result += ",";
        result += dayNames[i];
      }
    }
    return result;
  }

  String Control::getActiveMonthsString(const BitArray12& months) {
    String result;
    const char* monthNames[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    for (int i = 0; i < 12; i++) {
      if (months.get(i)) {
        if (result.length() > 0) result += ",";
        result += monthNames[i];
      }
    }
    return result;
  }

  void Control::setSchedules() {
    if (myDevices.empty()) return;
    Device& device = myDevices[currentDeviceIndex];

    static bool lastScheduleState = device.isScheduleEnabled;
    if (lastScheduleState != device.isScheduleEnabled) {
      if (!device.isScheduleEnabled) {

        for (auto& scenario : device.scheduleScenarios) {
          if (scenario.isActive) {
            logger.addLog("Deactivating all scenarios (schedule disabled)");
            collectionSettingsSchedule(false, scenario);
            scenario.isActive = false;
          }
        }
      }
      lastScheduleState = device.isScheduleEnabled;
    }

    if (!device.isScheduleEnabled) {
      return;
    }

    time_t currentDateTime = getCurrentTime();
    struct tm currentTime;
    localtime_r(&currentDateTime, &currentTime);
    int currentMinutes = currentTime.tm_hour * 60 + currentTime.tm_min;

    for (auto& scenario : device.scheduleScenarios) {

      if (!scenario.isUseSetting) {
        if (scenario.isActive) {
          String message = "Deactivating scenario '";
          message += scenario.description;
          message += "' (isUseSetting is now false)";
          logger.addLog(message);

          collectionSettingsSchedule(false, scenario);
          scenario.isActive = false;
        }
        continue;
      }

      time_t startDate = convertToTimeT(scenario.startDate);
      time_t endDate = convertToTimeT(scenario.endDate);
      time_t currentDate = convertOnlyDate(currentDateTime);

      if (currentDate < startDate || (endDate != 0 && currentDate > endDate)) {
        if (scenario.isActive) {
          String message = "Scenario '";
          message += scenario.description;
          message += "' expired";
          logger.addLog(message);

          collectionSettingsSchedule(false, scenario);
          scenario.isActive = false;
        }
        continue;
      }

      if (!scenario.months.get(currentTime.tm_mon)) {
        if (scenario.isActive) {
          String message = "Scenario '";
          message += scenario.description;
          message += "' inactive this month";
          logger.addLog(message);

          collectionSettingsSchedule(false, scenario);
          scenario.isActive = false;
        }
        continue;
      }

      if (!scenario.week.get(shiftWeekDay(currentTime.tm_wday))) {
        if (scenario.isActive) {
          String message = "Scenario '";
          message += scenario.description;
          message += "' inactive today";
          logger.addLog(message);

          collectionSettingsSchedule(false, scenario);
          scenario.isActive = false;
        }
        continue;
      }

      bool shouldBeActive = false;
      String activePeriod = "";
      for (auto& timePeriod : scenario.startEndTimes) {
        String startTimeStr = String(timePeriod.startTime);
        String endTimeStr = String(timePeriod.endTime);

        int startMinutes = startTimeStr.substring(0, 2).toInt() * 60 +
                           startTimeStr.substring(3, 5).toInt();
        int endMinutes = endTimeStr.substring(0, 2).toInt() * 60 +
                         endTimeStr.substring(3, 5).toInt();

        if (startTimeStr.length() < 5 || endTimeStr.length() < 5) {
          String message = "Invalid time format in scenario: ";
          message += scenario.description;
          logger.addLog(message);
          continue;
        }

        if (endMinutes < startMinutes) {

          shouldBeActive = (currentMinutes >= startMinutes) || (currentMinutes <= endMinutes);
        } else {

          shouldBeActive = (currentMinutes >= startMinutes && currentMinutes <= endMinutes);
        }

        if (shouldBeActive) {
          activePeriod = startTimeStr;
          activePeriod += "-";
          activePeriod += endTimeStr;
          break;
        }
      }

      if (shouldBeActive && !scenario.isActive) {
        String timeStr = String(currentTime.tm_hour);
        timeStr += ":";
        timeStr += (currentTime.tm_min < 10 ? "0" : "");
        timeStr += String(currentTime.tm_min);

        String message = "Activating scenario: ";
        message += scenario.description;
        message += " | Time: ";
        message += timeStr;
        message += " | Active period: ";
        message += activePeriod;

        int startMinutes = activePeriod.substring(0, 2).toInt() * 60 +
                           activePeriod.substring(3, 5).toInt();
        int endMinutes = activePeriod.substring(6, 8).toInt() * 60 +
                         activePeriod.substring(9, 11).toInt();
        if (endMinutes < startMinutes) {
          message += " (crosses midnight)";
        }

        message += " | Days: ";
        message += getActiveDaysString(scenario.week);
        message += " | Months: ";
        message += getActiveMonthsString(scenario.months);

        collectionSettingsSchedule(true, scenario);
        scenario.isActive = true;
      }
      else if (!shouldBeActive && scenario.isActive) {
        String message = "Deactivating scenario: ";
        message += scenario.description;
        message += " (time period ended)";
        logger.addLog(message);

        collectionSettingsSchedule(false, scenario);
        scenario.isActive = false;
      }
    }
  }

  void Control::setupControl() {
    if (myDevices.empty()) {
      logger.addLog("Error: No devices available");
      return;
    }

    if (currentDeviceIndex >= myDevices.size()) {
      currentDeviceIndex = 0;
      logger.addLog("Error: Invalid device index! Reset to 0");
    }

    Device& device = myDevices[currentDeviceIndex];

    for (auto& sensor : device.sensors) {
      if ((sensor.typeSensor.get(0) || sensor.typeSensor.get(1)) && sensor.dht == nullptr) {

        Relay* inputRelay = findRelayById(device, sensor.relayId);
        if (inputRelay && !inputRelay->isOutput) {

          sensor.dht = new DHT(inputRelay->pin, sensor.typeSensor.get(0) ? DHT11 : DHT22);
          sensor.dht->begin();

          delay(100);

          float testTemp = sensor.dht->readTemperature();
          float testHum = sensor.dht->readHumidity();

          String logMsg = "DHT инициализирован: pin " + String(inputRelay->pin) +
                          ", type " + (sensor.typeSensor.get(0) ? "DHT11" : "DHT22") +
                          ", test temp=" + String(testTemp) +
                          ", hum=" + String(testHum);
          logger.addLog(logMsg);

          if (isnan(testTemp) || isnan(testHum)) {
            logger.addLog("ВНИМАНИЕ: DHT возвращает ошибку при тестовом чтении!", 0);
          }
        } else {
          logger.addLog("Ошибка: Не найден вход для DHT сенсора ID " + String(sensor.sensorId), 0);
        }
      }
    }

    std::unordered_set<uint8_t> usedPins;
    for (auto& relay : device.relays) {
  #ifdef ESP8266
      if (relay.pin == 6 || relay.pin == 7 || relay.pin == 8 || relay.pin == 11) {
        if (callbackFunc) {
          callbackFunc("Warning: Skipping restricted pin " + String(relay.pin));
        }
        continue;
      }
  #endif

      if (usedPins.count(relay.pin)) {
        logger.addLog("Error: Pin " + String(relay.pin) + " is already used by another relay/input.");
        continue;
      }
      usedPins.insert(relay.pin);

      if (relay.isOutput) {

        pinMode(relay.pin, OUTPUT);
        digitalWrite(relay.pin, relay.statePin ? HIGH : LOW);

        logger.addLog("Relay init: pin " + String(relay.pin) +
                      ", state " + String(relay.statePin));

      } else {

        if (std::find(device.pins.begin(), device.pins.end(), relay.pin) == device.pins.end()) {
          logger.addLog("Pin " + String(relay.pin) + " not in allowed pins list.");
          continue;
        }

        Sensor* linkedSensor = nullptr;
        for (auto& sensor : device.sensors) {
          if (sensor.relayId == relay.id) {
            linkedSensor = &sensor;
            break;
          }
        }

        if (linkedSensor) {

          if (linkedSensor->typeSensor.get(3)) {
            if (!relay.isDigital) {
              logger.addLog("Warning: Forcing isDigital=true for TOUCH sensor on pin " + String(relay.pin));
              relay.isDigital = true;
            }
            pinMode(relay.pin, INPUT_PULLUP);
            touchStates[relay.pin] = TouchSensorState();
            logger.addLog("TOUCH_GND init: pin " + String(relay.pin));
          }
          else if (linkedSensor->typeSensor.get(0) || linkedSensor->typeSensor.get(1)) {

            if (!relay.isDigital) {
              logger.addLog("Warning: Forcing isDigital=true for DHT sensor on pin " + String(relay.pin));
              relay.isDigital = true;
            }

            logger.addLog("DHT-" + String(linkedSensor->typeSensor.get(0) ? "11" : "22") +
                          " pin configured: " + String(relay.pin));
          }
          else if (linkedSensor->typeSensor.get(2) || linkedSensor->typeSensor.get(4)) {
            if (relay.isDigital) {
              logger.addLog("Warning: Forcing isDigital=false for Analog sensor on pin " + String(relay.pin));
              relay.isDigital = false;
            }
            pinMode(relay.pin, INPUT);
            logger.addLog("NTC init: pin " + String(relay.pin));
          }

          else {
            if (!relay.isDigital) {
              logger.addLog("Warning: Forcing isDigital=true for generic digital input on pin " + String(relay.pin));
              relay.isDigital = true;
            }
            pinMode(relay.pin, INPUT);
            logger.addLog("Digital input init: pin " + String(relay.pin));
          }
        } else {

          if (!relay.isDigital) {
            logger.addLog("Warning: No sensor linked, forcing isDigital=true for generic input on pin " + String(relay.pin));
            relay.isDigital = true;
          }
          pinMode(relay.pin, INPUT);
          logger.addLog("Generic input init (no sensor): pin " + String(relay.pin));
        }
      }

      yield();
      delay(5);
    }

    logger.addLog("Control setup completed");
  }

  float Control::readNTCTemperature(const Sensor& sensor) {
    if (!sensor.typeSensor.get(2)) return -999.0;

    Relay* inputRelay = findRelayById(myDevices[currentDeviceIndex], sensor.relayId);

    if (!inputRelay || inputRelay->isOutput) {
      return -999.0;
    }

    int pin = inputRelay->pin;
    const int adc = analogRead(pin);
    const int mapped = map(adc, 0, 4095, 0, 1023);
    if (mapped == 0) return -999.0;

    const float resistance = sensor.serial_r / (1023.0 / mapped - 1);
    float temp = log(resistance / sensor.thermistor_r);
    temp /= 3950.0;
    temp += 1.0 / (25.0 + 273.15);
    temp = 1.0 / temp - 273.15 - 1;

    if (debug) {
      logger.addLog("NTC temp: " + String(temp));
    }

    return temp;
  }

  int Control::readAnalog(const Sensor& sensor) {

    if (!sensor.typeSensor.get(4)) {
      return -1;
    }

    Relay* inputRelay = findRelayById(myDevices[currentDeviceIndex], sensor.relayId);

    if (!inputRelay || inputRelay->isOutput) {
      return -1;
    }

    int adcValue = analogRead(inputRelay->pin);

    int mappedValue = map(adcValue, 0, 4095, 0, 255);

    return mappedValue;
  }

  bool Control::checkTouchSensor(uint8_t pin) {
    static const int DEBOUNCE_MS = 20;
    bool currentState = digitalRead(pin);

    auto& ts = touchStates[pin];
    unsigned long now = millis();

    if (currentState != ts.lastState) {
      ts.lastDebounceTime = now;
      ts.lastState = currentState;
    }

    if ((now - ts.lastDebounceTime) > DEBOUNCE_MS) {

      if (currentState == LOW) {
        ts.pressed = true;
        ts.pressStartTime = now;
        return true;
      } else {
        ts.pressed = false;
        return false;
      }
    }

    return ts.pressed;
  }

  void Control::readSensors() {

    static unsigned long lastDHTRead = 0;
    static unsigned long lastFastRead = 0;

    if (myDevices.empty()) return;
    if (currentDeviceIndex >= myDevices.size()) return;

    Device& device = myDevices[currentDeviceIndex];
    unsigned long now = millis();

    bool timeToReadFast = (now - lastFastRead >= 200);
    bool timeToReadDHT = (now - lastDHTRead >= 1000);

    for (auto& sensor : device.sensors) {
      if (!sensor.isUseSetting) continue;

      if (sensor.typeSensor.get(0) || sensor.typeSensor.get(1)) {

        if (timeToReadDHT) {
          if (sensor.dht) {
            float temp = sensor.dht->readTemperature();
            float hum  = sensor.dht->readHumidity();

            if (!isnan(temp)) sensor.currentValue = temp;
            if (!isnan(hum))  sensor.humidityValue = hum;
          }
        }
      }

      else {

        if (timeToReadFast) {
          if (sensor.typeSensor.get(2)) {
            sensor.currentValue = readNTCTemperature(sensor);
          }
          else if (sensor.typeSensor.get(3)) {
            Relay* inputRelay = findRelayById(device, sensor.relayId);
            if (inputRelay && !inputRelay->isOutput) {
              sensor.currentValue = checkTouchSensor(inputRelay->pin) ? 1.0f : 0.0f;
            }
          }
          else if (sensor.typeSensor.get(4)) {
            int analogVal = readAnalog(sensor);
            if (analogVal != -1) {
              sensor.currentValue = static_cast<float>(analogVal);
            } else {
              sensor.currentValue = -1.0f;
            }
          }
        }
      }
    }

    if (timeToReadFast) {
      lastFastRead = now;
    }
    if (timeToReadDHT) {
      lastDHTRead = now;
    }
  }

  String Control::currentStateSensors() {
    if (myDevices.empty()) return "No devices available";
    Device& device = myDevices[currentDeviceIndex];

    String result = "Состояние сенсоров устройства:\n";

    for (size_t i = 0; i < device.sensors.size(); i++) {
      Sensor& sensor = device.sensors[i];
      String msg = "Сенсор " + String(i) + ": ";

      if (sensor.typeSensor.get(2)) {
        msg += "Температура = " + String(sensor.currentValue, 2) + " °C";
      }
      else if (sensor.typeSensor.get(3)) {
        Relay* inputRelay = findRelayById(device, sensor.relayId);
        if (inputRelay && !inputRelay->isOutput) {
          msg += "Состояние = " + String(sensor.currentValue > 0.5 ? "НАЖАТО" : "ОТПУЩЕНО");
        } else {
          msg += "Ошибка: сенсор привязан к реле или не найден";
        }
      }
      else if (sensor.typeSensor.get(0) || sensor.typeSensor.get(1)) {
        msg += "Температура = " + String(sensor.currentValue, 2) + " °C, ";
        msg += "Влажность = " + String(sensor.humidityValue, 1) + " %";
      }
      else {
        msg += "Значение = " + String(sensor.currentValue, 2);
      }

      result += msg + "\n";
    }

    return result;
  }

 void Control::setSensorActions() {
    if (myDevices.empty()) return;
    Device& device = myDevices[currentDeviceIndex];

    static bool lastIsActionEnabled = false;
    static bool firstCall = true;

if (firstCall) {
    for (auto& action : device.actions) {
        action.wasTriggered = false;

        if (action.collectionSettings.get(1)) {
            for (auto& output : action.outputs) {
                if (output.isUseSetting) {
                    deviceManager.saveRelayStates(output.relayId);
                    #ifdef DEBUG_SENSOR_ACTIONS
                        logger.addLog("First run: Saved state for relay " + String(output.relayId) +
                                     " in action: " + String(action.description));
                    #endif
                }
            }
        }
    }
    firstCall = false;
    #ifdef DEBUG_SENSOR_ACTIONS
        logger.addLog("First run: All action triggers reset");
    #endif
}

    if (lastIsActionEnabled && !device.isActionEnabled) {
        logger.addLog("Actions disabled, resetting all triggers.");
        for (auto& action : device.actions) {
            action.wasTriggered = false;
                resetActionEffects(action, device);
        }
    }

    lastIsActionEnabled = device.isActionEnabled;

    if (!device.isActionEnabled) {
        return;
    }

    for (auto& action : device.actions) {
        if (!action.isUseSetting) {
            action.wasTriggered = false;
            continue;
        }

        Sensor* targetSensor = nullptr;
        for (auto& sensor : device.sensors) {
            if (sensor.sensorId == action.targetSensorId) {
                targetSensor = &sensor;
                break;
            }
        }

        if (!targetSensor || targetSensor->currentValue <= -998.0f) {
            continue;
        }

        float currentValue = action.isHumidity ? targetSensor->humidityValue : targetSensor->currentValue;

        bool shouldTrigger = false;
        bool shouldReset = false;

        if (action.actionMoreOrEqual) {
            shouldTrigger = (currentValue >= action.triggerValueMax);
            shouldReset = (currentValue < action.triggerValueMin);
        } else {
            shouldTrigger = (currentValue <= action.triggerValueMax);
            shouldReset = (currentValue > action.triggerValueMin);
        }

        if (shouldTrigger && !action.wasTriggered) {
            logger.addLog("Action TRIGGERED: " + String(action.description));

            if (action.collectionSettings.get(0)) {
                device.temperature.isUseSetting = true;
            }

            if (action.collectionSettings.get(1)) {
                for (auto& output : action.outputs) {

                    controlOutputs(output);

                }
            }

            if (action.collectionSettings.get(2)) {
                device.isTimersEnabled = true;
            }

            if (action.collectionSettings.get(3) && action.sendMsg.length() > 0) {
                logger.addLog(action.sendMsg, 2);
            }

            action.wasTriggered = true;
        }

        else if (shouldReset && action.wasTriggered) {
            logger.addLog("Action RESET: " + String(action.description));

            if (action.isReturnSetting) {

                if (action.collectionSettings.get(2)) {
                    device.isTimersEnabled = false;
                }

                if (action.collectionSettings.get(0)) {
                    device.temperature.isUseSetting = false;
                }

                if (action.collectionSettings.get(1)) {
                    for (auto& output : action.outputs) {
                        if (!output.isUseSetting || !output.isReturn) continue;

                        restoreRelayStates(output.relayId);

                        logger.addLog("Relay " + String(output.relayId) + " restored to previous state");
                    }
                }
            }

            action.wasTriggered = false;
        }
    }
}

void Control::resetActionEffects(Action& action, Device& device) {

    if (action.collectionSettings.get(2)) {
        device.isTimersEnabled = false;
    }

    if (action.collectionSettings.get(0)) {
        device.temperature.isUseSetting = false;
    }

    if (action.collectionSettings.get(1)) {
        for (auto& output : action.outputs) {
            if (output.isReturn && output.isUseSetting) {
                restoreRelayStates(output.relayId);
            }
        }
    }
}

  String Control::sendHelp() {
    if (myDevices.empty()) {
      return "Нет устройств для отображения справки\n";
    }

    const Device& currentDevice = myDevices[currentDeviceIndex];

    String helpMessage = "Доступные команды для устройства: ";
    helpMessage += currentDevice.nameDevice;
    helpMessage += "\n\n";

    int relayIndex = 1;
    for (const auto& relay : currentDevice.relays) {
      if (relay.isOutput) {
        String commandOn = "/on" + String(relayIndex);
        String commandOff = "/off" + String(relayIndex);

        helpMessage += commandOn + " " + commandOff + " - " + String(relay.description) + "\n";
        relayIndex++;
      }
    }

    helpMessage += "\nОбщие команды:\n";
    helpMessage += "/status - Показать текущий статус\n";
    helpMessage += "/pushOn /pushOff - Вкл/Выкл уведомления\n";
    helpMessage += "/log - показать лог\n";
    helpMessage += "/newtoken - Установить новый Bot токен\n";
    helpMessage += "/listUsers - Показать список пользователей\n";
    helpMessage += "/newUser - Добавить нового пользователя\n";
    helpMessage += "/delUser - Удалить пользователя\n";
    helpMessage += "/resetManual - Сбросить все реле в автоматический режим\n";
    helpMessage += "/resetFull - Сбросить все параметры\n";
    helpMessage += "/debug - Отладочная информация\n";
    helpMessage += "/sysinfo - Системная информация\n";

    if (currentDevice.temperature.isUseSetting) {
      helpMessage += "/temp - Информация о температуре\n";
      helpMessage += "/settemp [значение] - Установить температуру\n";
    }

    if (!currentDevice.timers.empty()) {
      helpMessage += "/timers - Показать таймеры\n";
    }

    if (!currentDevice.scheduleScenarios.empty()) {
      helpMessage += "/scenarios - Показать сценарии\n";
    }

    bool hasInputs = false;
    for (const auto& relay : currentDevice.relays) {
      if (!relay.isOutput) {
        hasInputs = true;
        break;
      }
    }
    if (hasInputs) {
      helpMessage += "/sensors - Показать данные сенсоров\n";
    }

  #if defined(ESP8266) || defined(ESP32)
    if (WiFi.status() == WL_CONNECTED) {
      helpMessage += "\nДоступ из сети: http://" + WiFi.localIP().toString() + "\n";
    }
  #endif

    return helpMessage;
  }

  String Control::sendStatus() {

    String ipAddress = "";
    String ssid = "";

  #if defined(ESP8266) || defined(ESP32)
    ipAddress = WiFi.localIP().toString();
    ssid = WiFi.SSID();
  #endif

    if (myDevices.empty()) {
      return "Нет устройств для отображения статуса\n";
    }

    const Device& currentDevice = myDevices[currentDeviceIndex];

    String helpText;

    helpText.reserve(1500);

    helpText = "Устройство: ";
    helpText += currentDevice.nameDevice;
    helpText += "\n";
    helpText += "Текущий статус реле:\n";

    int relayIndex = 1;
    for (const auto& relay : currentDevice.relays) {
      if (relay.isOutput) {
        String commandOn = "/on" + String(relayIndex);
        String commandOff = "/off" + String(relayIndex);
        String mode_ = relay.manualMode ? "Ручной" : "Авто";
        String state_ = relay.statePin ? "Вкл" : "Выкл";

        helpText += commandOn + " " + commandOff + " - " + String(relay.description) +
                    " (Режим: " + mode_ + ", " + state_ + ")\n";
        relayIndex++;
      }
    }

    helpText += "\nКоманды:\n";
    helpText += "/resetManual - Сбросить в автоматический режим\n";
    helpText += "/status - Текущий статус\n";
    helpText += "/help - Справка по командам\n\n";

  #if defined(ESP8266) || defined(ESP32)
    helpText += "Доступ из сети: http://" + ipAddress + "\n";
    helpText += "Подключено к Wi-Fi: " + ssid + "\n";
  #endif

    return helpText;
  }

  String Control::getSensorStatus() {
    return currentStateSensors();
  }

  String Control::getSystemStatus() {
    return sendStatus();
  }

  String Control::getHelp() {
    return sendHelp();
  }

  void Control::processCommand(const String& command) {
    manualWork(command);
  }
