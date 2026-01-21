#include "DeviceManager.h"
  #include <cstring>

  void DeviceManager::initializeDevice(const char* name, bool activ, bool isNewDevice) {
    if (!isNewDevice) {
      if (!myDevices.empty()) {
        myDevices.clear();
      }
    }

    myDevices.emplace_back();
    Device& newDevice = myDevices.back();

    strncpy_safe(newDevice.nameDevice, name, MAX_DESCRIPTION_LENGTH);
    newDevice.isSelected = activ;
    newDevice.isTimersEnabled = false;
    newDevice.isEncyclateTimers = true;
    newDevice.isScheduleEnabled = false;
    newDevice.isActionEnabled = false;

  #ifdef ESP32
  #ifdef CONFIG_IDF_TARGET_ESP32S2

    newDevice.pins = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 18, 21, 33, 35};
  #elif defined(CONFIG_IDF_TARGET_ESP32)
    newDevice.pins = { 0, 1, 2, 3, 4, 5, 12, 13, 14, 15, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33 };
  #else
    newDevice.pins = {1, 2, 3, 4, 5, 6, 7, 8, 9};
  #endif
  #elif defined(ESP8266)
    newDevice.pins = {1, 2, 3, 4, 5, 12, 13, 14, 17};
  #else
    newDevice.pins = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21};
  #endif

    int nextId = 0;

  #ifdef CONFIG_IDF_TARGET_ESP32S2

    Serial.println("[DeviceManager] Initializing for ESP32-S2 with custom pins.");

    const int output_pins[] = {16, 17, 18, 21};
    const bool output_manual_mode[] = {true, false, true, false};
    const bool output_state_pin[] = {true, false, true, false};

    for (int i = 0; i < 4; i++) {
      Relay relay;
      relay.id = nextId++;
      relay.pin = output_pins[i];
      relay.manualMode = output_manual_mode[i];
      relay.statePin = output_state_pin[i];
      relay.isOutput = true;
      relay.isDigital = true;
      relay.lastState = false;
      relay.isPwm = false;
      relay.pwm = 0;
      snprintf(relay.description, MAX_DESCRIPTION_LENGTH, "Выход_%d", i + 1);
      newDevice.relays.push_back(relay);
    }

    Relay dhtInput;
    dhtInput.id = nextId++;
    dhtInput.pin = 35;
    dhtInput.manualMode = false;
    dhtInput.isOutput = false;
    dhtInput.isDigital = true;
    dhtInput.statePin = false;
    dhtInput.lastState = false;
    strncpy_safe(dhtInput.description, "DHT-11 Датчик", MAX_DESCRIPTION_LENGTH);
    newDevice.relays.push_back(dhtInput);

    Relay currentInput;
    currentInput.id = nextId++;
    currentInput.pin = 33;
    currentInput.manualMode = false;
    currentInput.isOutput = false;
    currentInput.isDigital = false;
    currentInput.statePin = false;
    currentInput.lastState = false;
    strncpy_safe(currentInput.description, "Вход датчик тока", MAX_DESCRIPTION_LENGTH);
    newDevice.relays.push_back(currentInput);

  #else

    Serial.println("[DeviceManager] Initializing for a generic board with default pins.");

    const int output_pins[] = {3, 18, 19, 21};
    const bool output_manual_mode[] = {true, false, true, false};
    const bool output_state_pin[] = {true, false, true, false};

    for (int i = 0; i < 4; i++) {
      Relay relay;
      relay.id = nextId++;
      relay.pin = output_pins[i];
      relay.manualMode = output_manual_mode[i];
      relay.statePin = output_state_pin[i];
      relay.isOutput = true;
      relay.isDigital = true;
      relay.lastState = false;
      relay.isPwm = false;
      relay.pwm = 0;
      snprintf(relay.description, MAX_DESCRIPTION_LENGTH, "Выход_%d", i + 1);
      newDevice.relays.push_back(relay);
    }

    Relay dhtInput;
    dhtInput.id = nextId++;
    dhtInput.pin = 23;
    dhtInput.manualMode = false;
    dhtInput.isOutput = false;
    dhtInput.isDigital = true;
    dhtInput.statePin = false;
    dhtInput.lastState = false;
    strncpy_safe(dhtInput.description, "DHT-11 Датчик", MAX_DESCRIPTION_LENGTH);
    newDevice.relays.push_back(dhtInput);

    Relay currentInput;
    currentInput.id = nextId++;
    currentInput.pin = 33;
    currentInput.manualMode = false;
    currentInput.isOutput = false;
    currentInput.isDigital = false;
    currentInput.statePin = false;
    currentInput.lastState = false;
    strncpy_safe(currentInput.description, "Вход датчик тока", MAX_DESCRIPTION_LENGTH);
    newDevice.relays.push_back(currentInput);
  #endif

    Sensor dhtSensor;
    strncpy_safe(dhtSensor.description, "Сенсор DHT11", MAX_DESCRIPTION_LENGTH);
    dhtSensor.isUseSetting = true;
    dhtSensor.sensorId = nextId++;
    dhtSensor.relayId = 4;
    dhtSensor.typeSensor.clear();
    dhtSensor.typeSensor.set(0, true);
    dhtSensor.serial_r = 20000;
    dhtSensor.thermistor_r = 10000;
    dhtSensor.currentValue = 0.0;
    dhtSensor.humidityValue = 0.0;
    dhtSensor.dht = nullptr;
    newDevice.sensors.push_back(dhtSensor);

    Sensor currentSensor;
    strncpy_safe(currentSensor.description, "Датчик тока", MAX_DESCRIPTION_LENGTH);
    currentSensor.isUseSetting = true;
    currentSensor.sensorId = nextId++;
    currentSensor.relayId = 5;
    currentSensor.typeSensor.clear();
    currentSensor.typeSensor.set(4, true);
    currentSensor.serial_r = 20000;
    currentSensor.thermistor_r = 10000;
    currentSensor.currentValue = 0.0;
    currentSensor.humidityValue = 0.0;
    newDevice.sensors.push_back(currentSensor);

    Action touchAction;
    strncpy_safe(touchAction.description, "Действие - превышение тока", MAX_DESCRIPTION_LENGTH);
    touchAction.isUseSetting = true;
    touchAction.targetSensorId = currentSensor.sensorId;
    touchAction.triggerValueMax = 1;
    touchAction.triggerValueMin = 0.5;
    touchAction.isHumidity = false;
    touchAction.actionMoreOrEqual = true;
    touchAction.isReturnSetting = true;
    touchAction.wasTriggered = false;
    touchAction.collectionSettings.clear();
    touchAction.collectionSettings.set(1, true);

    OutPower defaultTouchOutput;
    defaultTouchOutput.isUseSetting = true;
    defaultTouchOutput.relayId = newDevice.relays[0].id;
    defaultTouchOutput.statePin = false;
    defaultTouchOutput.lastState = false;
    defaultTouchOutput.isReturn = true;
    strncpy_safe(defaultTouchOutput.description, "Touch Action Output", MAX_DESCRIPTION_LENGTH);
    touchAction.outputs.push_back(defaultTouchOutput);
    newDevice.actions.push_back(touchAction);

    ScheduleScenario scenario;
    strncpy_safe(scenario.description, "Мой первый сценарий 1", MAX_DESCRIPTION_LENGTH);
    scenario.isUseSetting = false;
    scenario.isActive = false;
    scenario.collectionSettings.clear();
    scenario.collectionSettings.set(3, true);
    strncpy_safe(scenario.startDate, "2012-12-12", MAX_DATE_LENGTH);
    strncpy_safe(scenario.endDate, "2222-12-12", MAX_DATE_LENGTH);

    startEndTime timeInterval;
    strncpy_safe(timeInterval.startTime, "08:00", MAX_TIME_LENGTH);
    strncpy_safe(timeInterval.endTime, "18:00", MAX_TIME_LENGTH);
    scenario.startEndTimes.push_back(timeInterval);
    scenario.week.bits = 0x7F;
    scenario.months.bits = 0xFFF;

    scenario.initialStateRelay.isUseSetting = true;
    scenario.initialStateRelay.relayId = newDevice.relays[0].id;
    scenario.initialStateRelay.statePin = true;
    scenario.initialStateRelay.lastState = false;
    strncpy_safe(scenario.initialStateRelay.description, "Initial State", MAX_DESCRIPTION_LENGTH);

    scenario.endStateRelay.isUseSetting = false;
    scenario.endStateRelay.relayId = newDevice.relays[0].id;
    scenario.endStateRelay.statePin = false;
    scenario.endStateRelay.lastState = false;
    strncpy_safe(scenario.endStateRelay.description, "End State", MAX_DESCRIPTION_LENGTH);

    scenario.temperatureUpdated = false;
    scenario.timersExecuted = false;
    scenario.initialStateApplied = false;
    scenario.endStateApplied = false;
    scenario.scenarioProcessed = false;
    newDevice.scheduleScenarios.push_back(scenario);

    newDevice.temperature.isUseSetting = false;
    newDevice.temperature.relayId = newDevice.relays[0].id;
    newDevice.temperature.lastState = false;
    newDevice.temperature.sensorId = dhtSensor.sensorId;
    newDevice.temperature.setTemperature = 22;
    newDevice.temperature.currentTemp = 0.0;
    newDevice.temperature.isSmoothly = false;
    newDevice.temperature.isIncrease = true;
    newDevice.temperature.collectionSettings.clear();
    newDevice.temperature.collectionSettings.set(0, true);
    newDevice.temperature.selectedPidIndex = 0;

    Pid pid1, pid2, pid3;

    strncpy_safe(pid1.description, "Стандартный", MAX_DESCRIPTION_LENGTH);
    strncpy_safe(pid1.descriptionDetailed, "Сбалансированный набор. Хорошее соотношение скорости реакции и стабильности. Подходит для большинства систем.", MAX_TXT_DESCRIPTION_LENGTH);
    pid1.Kp = 2.0; pid1.Ki = 0.5; pid1.Kd = 1.0;

    strncpy_safe(pid2.description, "Быстрый", MAX_DESCRIPTION_LENGTH);
    strncpy_safe(pid2.descriptionDetailed, "Быстрый набор с высоким Kp. Реагирует резко на изменения температуры. Может вызывать перерегулирование (overshoot). Идеален для малых инерционных систем.", MAX_TXT_DESCRIPTION_LENGTH);
    pid2.Kp = 1.5; pid2.Ki = 0.4; pid2.Kd = 0.9;

    strncpy_safe(pid3.description, "Плавный", MAX_DESCRIPTION_LENGTH);
    strncpy_safe(pid3.descriptionDetailed, "Плавный набор с низким Kp. Медленно и плавно достигает заданной температуры, минимизируя перерегулирование. Подходит для больших инерционных систем (например, отопление дома).", MAX_TXT_DESCRIPTION_LENGTH);
    pid3.Kp = 1.0; pid3.Ki = 0.3; pid3.Kd = 0.8;

    newDevice.pids.push_back(pid1);
    newDevice.pids.push_back(pid2);
    newDevice.pids.push_back(pid3);

    Timer timer;
    timer.isUseSetting = true;
    strncpy_safe(timer.time, "00:00:05", MAX_TIME_LENGTH);
    timer.collectionSettings.clear();
    timer.collectionSettings.set(1, true);

    timer.initialStateRelay.isUseSetting = true;
    timer.initialStateRelay.relayId = newDevice.relays[0].id;
    timer.initialStateRelay.statePin = true;
    timer.initialStateRelay.lastState = false;
    strncpy_safe(timer.initialStateRelay.description, "Initial Timer Power", MAX_DESCRIPTION_LENGTH);

    timer.endStateRelay.isUseSetting = true;
    timer.endStateRelay.relayId = newDevice.relays[0].id;
    timer.endStateRelay.statePin = false;
    timer.endStateRelay.lastState = false;
    strncpy_safe(timer.endStateRelay.description, "End Timer Power", MAX_DESCRIPTION_LENGTH);

    newDevice.timers.push_back(timer);

    Serial.printf("Free heap after device initialization: %d\n", ESP.getFreeHeap());
  }

  String DeviceManager::serializeDevice(const Device& device) {

    DynamicJsonDocument doc(8192);

    doc["nameDevice"] = device.nameDevice;
    doc["isSelected"] = device.isSelected;

    JsonArray relays = doc.createNestedArray("relays");
    for (const auto& relay : device.relays) {
      JsonObject relayObj = relays.createNestedObject();
      relayObj["id"] = relay.id;
      relayObj["pin"] = relay.pin;
      relayObj["isPwm"] = relay.isPwm;
      relayObj["manualMode"] = relay.manualMode;
      relayObj["statePin"] = relay.statePin;
      relayObj["isOutput"] = relay.isOutput;
      relayObj["isDigital"] = relay.isDigital;
      relayObj["lastState"] = relay.lastState;
      relayObj["description"] = relay.description;
    }

    JsonArray pins = doc.createNestedArray("pins");
    for (const auto& pin : device.pins) {
      pins.add(pin);
    }

    JsonArray sensors = doc.createNestedArray("sensors");
    for (const auto& sensor : device.sensors) {
      JsonObject sensorObj = sensors.createNestedObject();

      sensorObj["description"] = sensor.description;
      sensorObj["isUseSetting"] = sensor.isUseSetting;
      sensorObj["sensorId"] = sensor.sensorId;
      sensorObj["relayId"] = sensor.relayId;

      JsonArray typeSensor = sensorObj.createNestedArray("typeSensor");
      for (int i = 0; i < 7; i++) {
        typeSensor.add(sensor.typeSensor.get(i));
      }

      sensorObj["serial_r"] = sensor.serial_r;
      sensorObj["thermistor_r"] = sensor.thermistor_r;
      sensorObj["currentValue"] = sensor.currentValue;
      sensorObj["humidityValue"] = sensor.humidityValue;
    }

    JsonArray actions = doc.createNestedArray("actions");
    for (const auto& action : device.actions) {
      JsonObject actionObj = actions.createNestedObject();

      actionObj["description"] = action.description;
      actionObj["isUseSetting"] = action.isUseSetting;
      actionObj["targetSensorId"] = action.targetSensorId;
      actionObj["triggerValueMax"] = action.triggerValueMax;
      actionObj["triggerValueMin"] = action.triggerValueMin;
      actionObj["isHumidity"] = action.isHumidity;
      actionObj["actionMoreOrEqual"] = action.actionMoreOrEqual;
      actionObj["isReturnSetting"] = action.isReturnSetting;
      actionObj["wasTriggered"] = action.wasTriggered;
      actionObj["sendMsg"] = action.sendMsg;

      JsonArray collectionSettings = actionObj.createNestedArray("collectionSettings");
      for (int i = 0; i < 4; i++) {
        collectionSettings.add(action.collectionSettings.get(i));
      }

      JsonArray outputs = actionObj.createNestedArray("outputs");
      for (const auto& output : action.outputs) {
        JsonObject outputObj = outputs.createNestedObject();
        outputObj["isUseSetting"] = output.isUseSetting;
        outputObj["relayId"] = output.relayId;
        outputObj["statePin"] = output.statePin;
        outputObj["lastState"] = output.lastState;
        outputObj["isReturn"] = output.isReturn;
        outputObj["description"] = output.description;
      }
    }

    JsonArray scheduleScenarios = doc.createNestedArray("scheduleScenarios");
    for (const auto& scenario : device.scheduleScenarios) {
      JsonObject scenarioObj = scheduleScenarios.createNestedObject();
      scenarioObj["isUseSetting"] = scenario.isUseSetting;
      scenarioObj["description"] = scenario.description;
      scenarioObj["isActive"] = scenario.isActive;

      JsonArray collectionSettings = scenarioObj.createNestedArray("collectionSettings");
      for (int i = 0; i < 4; i++) {
        collectionSettings.add(scenario.collectionSettings.get(i));
      }

      scenarioObj["startDate"] = scenario.startDate;
      scenarioObj["endDate"] = scenario.endDate;

      JsonArray startEndTimes = scenarioObj.createNestedArray("startEndTimes");
      for (const auto& timeInterval : scenario.startEndTimes) {
        JsonObject intervalObj = startEndTimes.createNestedObject();
        intervalObj["startTime"] = timeInterval.startTime;
        intervalObj["endTime"] = timeInterval.endTime;
      }

      JsonArray week = scenarioObj.createNestedArray("week");
      for (int i = 0; i < 7; i++) {
        week.add(scenario.week.get(i));
      }

      JsonArray months = scenarioObj.createNestedArray("months");
      for (int i = 0; i < 12; i++) {
        months.add(scenario.months.get(i));
      }

      JsonObject initialStateRelay = scenarioObj.createNestedObject("initialStateRelay");
      initialStateRelay["isUseSetting"] = scenario.initialStateRelay.isUseSetting;
      initialStateRelay["relayId"] = scenario.initialStateRelay.relayId;
      initialStateRelay["statePin"] = scenario.initialStateRelay.statePin;
      initialStateRelay["lastState"] = scenario.initialStateRelay.lastState;
      initialStateRelay["description"] = scenario.initialStateRelay.description;

      JsonObject endStateRelay = scenarioObj.createNestedObject("endStateRelay");
      endStateRelay["isUseSetting"] = scenario.endStateRelay.isUseSetting;
      endStateRelay["relayId"] = scenario.endStateRelay.relayId;
      endStateRelay["statePin"] = scenario.endStateRelay.statePin;
      endStateRelay["lastState"] = scenario.endStateRelay.lastState;
      endStateRelay["description"] = scenario.endStateRelay.description;
    }

    JsonObject temperature = doc.createNestedObject("temperature");
    temperature["isUseSetting"] = device.temperature.isUseSetting;
    temperature["relayId"] = device.temperature.relayId;
    temperature["lastState"] = device.temperature.lastState;
    temperature["sensorId"] = device.temperature.sensorId;
    temperature["setTemperature"] = device.temperature.setTemperature;
    temperature["currentTemp"] = device.temperature.currentTemp;
    temperature["isSmoothly"] = device.temperature.isSmoothly;
    temperature["isIncrease"] = device.temperature.isIncrease;

    JsonArray tempCollectionSettings = temperature.createNestedArray("collectionSettings");
    for (int i = 0; i < 4; i++) {
      tempCollectionSettings.add(device.temperature.collectionSettings.get(i));
    }

    temperature["selectedPidIndex"] = device.temperature.selectedPidIndex;

    JsonArray pidsArray = doc.createNestedArray("pids");
    for (const auto& pid : device.pids) {
      JsonObject pidObject = pidsArray.createNestedObject();
      pidObject["description"] = pid.description;
      pidObject["descriptionDetailed"] = pid.descriptionDetailed;
      pidObject["Kp"] = pid.Kp;
      pidObject["Ki"] = pid.Ki;
      pidObject["Kd"] = pid.Kd;
    }

    JsonArray timers = doc.createNestedArray("timers");
    for (const auto& timer : device.timers) {
      JsonObject timerObj = timers.createNestedObject();
      timerObj["isUseSetting"] = timer.isUseSetting;
      timerObj["time"] = timer.time;

      JsonArray collectionSettings = timerObj.createNestedArray("collectionSettings");
      for (int i = 0; i < 4; i++) {
        collectionSettings.add(timer.collectionSettings.get(i));
      }

      JsonObject initialStateRelay = timerObj.createNestedObject("initialStateRelay");
      initialStateRelay["isUseSetting"] = timer.initialStateRelay.isUseSetting;
      initialStateRelay["relayId"] = timer.initialStateRelay.relayId;
      initialStateRelay["statePin"] = timer.initialStateRelay.statePin;
      initialStateRelay["lastState"] = timer.initialStateRelay.lastState;
      initialStateRelay["description"] = timer.initialStateRelay.description;

      JsonObject endStateRelay = timerObj.createNestedObject("endStateRelay");
      endStateRelay["isUseSetting"] = timer.endStateRelay.isUseSetting;
      endStateRelay["relayId"] = timer.endStateRelay.relayId;
      endStateRelay["statePin"] = timer.endStateRelay.statePin;
      endStateRelay["lastState"] = timer.endStateRelay.lastState;
      endStateRelay["description"] = timer.endStateRelay.description;
    }

    doc["isTimersEnabled"] = device.isTimersEnabled;
    doc["isEncyclateTimers"] = device.isEncyclateTimers;
    doc["isScheduleEnabled"] = device.isScheduleEnabled;
    doc["isActionEnabled"] = device.isActionEnabled;

    String output;
    serializeJson(doc, output);
    return output;
  }

  bool DeviceManager::deserializeDevice(JsonObject doc, Device& device) {

    if (doc.containsKey("nameDevice")) {
      strncpy_safe(device.nameDevice, doc["nameDevice"], MAX_DESCRIPTION_LENGTH);
    }

    if (doc.containsKey("isSelected")) {
      device.isSelected = doc["isSelected"].as<bool>();
    }

    if (doc.containsKey("relays")) {
      device.relays.clear();
      JsonArray relays = doc["relays"];
      for (JsonObject relayObj : relays) {
        Relay relay;
        if (relayObj.containsKey("id")) relay.id = relayObj["id"];
        if (relayObj.containsKey("pin")) relay.pin = relayObj["pin"];
        if (relayObj.containsKey("manualMode")) relay.manualMode = relayObj["manualMode"].as<bool>();
        if (relayObj.containsKey("statePin")) relay.statePin = relayObj["statePin"].as<bool>();
        if (relayObj.containsKey("lastState")) relay.lastState = relayObj["lastState"].as<bool>();
        if (relayObj.containsKey("isOutput")) relay.isOutput = relayObj["isOutput"].as<bool>();
        if (relayObj.containsKey("isPwm")) relay.isPwm = relayObj["isPwm"].as<bool>();
        if (relayObj.containsKey("isDigital")) relay.isDigital = relayObj["isDigital"].as<bool>();
        if (relayObj.containsKey("description")) {
          strncpy_safe(relay.description, relayObj["description"], MAX_DESCRIPTION_LENGTH);
        }
        device.relays.push_back(relay);
      }
    }

    if (doc.containsKey("pins")) {
      device.pins.clear();
      JsonArray pins = doc["pins"];
      for (JsonVariant pin : pins) {
        device.pins.push_back(pin.as<uint8_t>());
      }
    }

    if (doc.containsKey("sensors")) {
      JsonArray sensorsJson = doc["sensors"];
      std::vector<Sensor> newSensors;

      for (JsonObject sensorObj : sensorsJson) {
        Sensor sensor;

        if (sensorObj.containsKey("description")) {
          strncpy_safe(sensor.description, sensorObj["description"], MAX_DESCRIPTION_LENGTH);
        }
        if (sensorObj.containsKey("isUseSetting")) sensor.isUseSetting = sensorObj["isUseSetting"].as<bool>();
        if (sensorObj.containsKey("sensorId")) sensor.sensorId = sensorObj["sensorId"];
        if (sensorObj.containsKey("relayId")) sensor.relayId = sensorObj["relayId"];

        if (sensorObj.containsKey("typeSensor")) {
          JsonArray typeSensor = sensorObj["typeSensor"];
          for (int i = 0; i < 7 && i < typeSensor.size(); i++) {
            sensor.typeSensor.set(i, typeSensor[i].as<bool>());
          }
        }

        if (sensorObj.containsKey("serial_r")) sensor.serial_r = sensorObj["serial_r"];
        if (sensorObj.containsKey("thermistor_r")) sensor.thermistor_r = sensorObj["thermistor_r"];

        newSensors.push_back(sensor);
      }

      noInterrupts();
      device.sensors = std::move(newSensors);
      interrupts();
    }

    if (doc.containsKey("actions")) {
      JsonArray actionsJson = doc["actions"];
      std::vector<Action> newActions;

      for (JsonObject actionObj : actionsJson) {
        Action action;

        if (actionObj.containsKey("description")) {
          strncpy_safe(action.description, actionObj["description"], MAX_DESCRIPTION_LENGTH);
        }
        if (actionObj.containsKey("isUseSetting")) action.isUseSetting = actionObj["isUseSetting"].as<bool>();
        if (actionObj.containsKey("targetSensorId")) action.targetSensorId = actionObj["targetSensorId"];
        if (actionObj.containsKey("triggerValueMax")) action.triggerValueMax = actionObj["triggerValueMax"];
        if (actionObj.containsKey("triggerValueMin")) action.triggerValueMin = actionObj["triggerValueMin"];
        if (actionObj.containsKey("isHumidity")) action.isHumidity = actionObj["isHumidity"].as<bool>();
        if (actionObj.containsKey("actionMoreOrEqual")) action.actionMoreOrEqual = actionObj["actionMoreOrEqual"].as<bool>();
        if (actionObj.containsKey("isReturnSetting")) action.isReturnSetting = actionObj["isReturnSetting"].as<bool>();
        if (actionObj.containsKey("wasTriggered")) action.wasTriggered = actionObj["wasTriggered"].as<bool>();
        if (actionObj.containsKey("sendMsg")) action.sendMsg = actionObj["sendMsg"].as<String>();

        if (actionObj.containsKey("collectionSettings")) {
          JsonArray collectionSettings = actionObj["collectionSettings"];
          for (int i = 0; i < 4 && i < collectionSettings.size(); i++) {
            action.collectionSettings.set(i, collectionSettings[i].as<bool>());
          }
        }

        if (actionObj.containsKey("outputs")) {
          JsonArray outputsJson = actionObj["outputs"];
          action.outputs.clear();
          for (JsonObject outputObj : outputsJson) {
            OutPower output;
            if (outputObj.containsKey("isUseSetting")) output.isUseSetting = outputObj["isUseSetting"].as<bool>();
            if (outputObj.containsKey("relayId")) output.relayId = outputObj["relayId"];
            if (outputObj.containsKey("statePin")) output.statePin = outputObj["statePin"].as<bool>();
            if (outputObj.containsKey("lastState")) output.lastState = outputObj["lastState"].as<bool>();
            if (outputObj.containsKey("isReturn")) output.isReturn = outputObj["isReturn"].as<bool>();
            if (outputObj.containsKey("description")) {
              strncpy_safe(output.description, outputObj["description"], MAX_DESCRIPTION_LENGTH);
            }
            action.outputs.push_back(output);
          }
        }

        newActions.push_back(action);
      }

      noInterrupts();
      device.actions = std::move(newActions);
      interrupts();
    }

    if (doc.containsKey("scheduleScenarios")) {
      device.scheduleScenarios.clear();
      JsonArray scenarios = doc["scheduleScenarios"];
      for (JsonObject scenarioObj : scenarios) {
        ScheduleScenario scenario;
        scenario.temperatureUpdated = false;
        scenario.timersExecuted = false;
        scenario.initialStateApplied = false;
        scenario.endStateApplied = false;
        scenario.scenarioProcessed = false;

        if (scenarioObj.containsKey("isUseSetting")) {
          scenario.isUseSetting = scenarioObj["isUseSetting"].as<bool>();
        }
        if (scenarioObj.containsKey("description")) {
          strncpy_safe(scenario.description, scenarioObj["description"], MAX_DESCRIPTION_LENGTH);
        }
        if (scenarioObj.containsKey("isActive")) {
          scenario.isActive = scenarioObj["isActive"].as<bool>();
        }

        if (scenarioObj.containsKey("collectionSettings")) {
          JsonArray collectionSettings = scenarioObj["collectionSettings"];
          for (int i = 0; i < 4 && i < collectionSettings.size(); i++) {
            scenario.collectionSettings.set(i, collectionSettings[i].as<bool>());
          }
        }

        if (scenarioObj.containsKey("startDate")) {
          strncpy_safe(scenario.startDate, scenarioObj["startDate"], MAX_DATE_LENGTH);
        }
        if (scenarioObj.containsKey("endDate")) {
          strncpy_safe(scenario.endDate, scenarioObj["endDate"], MAX_DATE_LENGTH);
        }

        if (scenarioObj.containsKey("startEndTimes")) {
          JsonArray startEndTimes = scenarioObj["startEndTimes"];
          for (JsonObject intervalObj : startEndTimes) {
            startEndTime timeInterval;
            if (intervalObj.containsKey("startTime")) {
              strncpy_safe(timeInterval.startTime, intervalObj["startTime"], MAX_TIME_LENGTH);
            }
            if (intervalObj.containsKey("endTime")) {
              strncpy_safe(timeInterval.endTime, intervalObj["endTime"], MAX_TIME_LENGTH);
            }
            scenario.startEndTimes.push_back(timeInterval);
          }
        }

        if (scenarioObj.containsKey("week")) {
          JsonArray week = scenarioObj["week"];
          for (int i = 0; i < 7 && i < week.size(); i++) {
            scenario.week.set(i, week[i].as<bool>());
          }
        }

        if (scenarioObj.containsKey("months")) {
          JsonArray months = scenarioObj["months"];
          for (int i = 0; i < 12 && i < months.size(); i++) {
            scenario.months.set(i, months[i].as<bool>());
          }
        }

        if (scenarioObj.containsKey("initialStateRelay")) {
          JsonObject initialStateRelay = scenarioObj["initialStateRelay"];
          if (initialStateRelay.containsKey("isUseSetting")) {
            scenario.initialStateRelay.isUseSetting = initialStateRelay["isUseSetting"].as<bool>();
          }
          if (initialStateRelay.containsKey("relayId")) {
            scenario.initialStateRelay.relayId = initialStateRelay["relayId"];
          }
          if (initialStateRelay.containsKey("statePin")) {
            scenario.initialStateRelay.statePin = initialStateRelay["statePin"].as<bool>();
          }
          if (initialStateRelay.containsKey("lastState")) {
            scenario.initialStateRelay.lastState = initialStateRelay["lastState"].as<bool>();
          }
          if (initialStateRelay.containsKey("description")) {
            strncpy_safe(scenario.initialStateRelay.description, initialStateRelay["description"], MAX_DESCRIPTION_LENGTH);
          }
        }

        if (scenarioObj.containsKey("endStateRelay")) {
          JsonObject endStateRelay = scenarioObj["endStateRelay"];
          if (endStateRelay.containsKey("isUseSetting")) {
            scenario.endStateRelay.isUseSetting = endStateRelay["isUseSetting"].as<bool>();
          }
          if (endStateRelay.containsKey("relayId")) {
            scenario.endStateRelay.relayId = endStateRelay["relayId"];
          }
          if (endStateRelay.containsKey("statePin")) {
            scenario.endStateRelay.statePin = endStateRelay["statePin"].as<bool>();
          }
          if (endStateRelay.containsKey("lastState")) {
            scenario.endStateRelay.lastState = endStateRelay["lastState"].as<bool>();
          }
          if (endStateRelay.containsKey("description")) {
            strncpy_safe(scenario.endStateRelay.description, endStateRelay["description"], MAX_DESCRIPTION_LENGTH);
          }
        }

        device.scheduleScenarios.push_back(scenario);
      }
    }

    if (doc.containsKey("temperature")) {
      JsonObject temperature = doc["temperature"];
      if (temperature.containsKey("isUseSetting")) {
        device.temperature.isUseSetting = temperature["isUseSetting"].as<bool>();
      }
      if (temperature.containsKey("relayId")) {
        device.temperature.relayId = temperature["relayId"];
      }
      if (temperature.containsKey("lastState")) {
        device.temperature.lastState = temperature["lastState"].as<bool>();
      }
      if (temperature.containsKey("sensorId")) {
        device.temperature.sensorId = temperature["sensorId"];
      }
      if (temperature.containsKey("setTemperature")) {
        device.temperature.setTemperature = temperature["setTemperature"];
      }
      if (temperature.containsKey("currentTemp")) {
        device.temperature.currentTemp = temperature["currentTemp"];
      }
      if (temperature.containsKey("isSmoothly")) {
        device.temperature.isSmoothly = temperature["isSmoothly"].as<bool>();
      }
      if (temperature.containsKey("isIncrease")) {
        device.temperature.isIncrease = temperature["isIncrease"].as<bool>();
      }
      if (temperature.containsKey("collectionSettings")) {
        JsonArray collectionSettings = temperature["collectionSettings"];
        for (int i = 0; i < 4 && i < collectionSettings.size(); i++) {
          device.temperature.collectionSettings.set(i, collectionSettings[i].as<bool>());
        }
      }
      if (temperature.containsKey("selectedPidIndex")) {
        device.temperature.selectedPidIndex = temperature["selectedPidIndex"];
      }
    }

    if (doc.containsKey("pids")) {
      device.pids.clear();
      JsonArray pidsArray = doc["pids"].as<JsonArray>();
      for (JsonObject pidObject : pidsArray) {
        Pid pid;
        if (pidObject.containsKey("description")) {
          strncpy_safe(pid.description, pidObject["description"], MAX_DESCRIPTION_LENGTH);
        }
        if (pidObject.containsKey("descriptionDetailed")) {

          strncpy_safe(pid.descriptionDetailed, pidObject["descriptionDetailed"], MAX_TXT_DESCRIPTION_LENGTH);

        }
        if (pidObject.containsKey("Kp")) pid.Kp = pidObject["Kp"];
        if (pidObject.containsKey("Ki")) pid.Ki = pidObject["Ki"];
        if (pidObject.containsKey("Kd")) pid.Kd = pidObject["Kd"];
        device.pids.push_back(pid);
      }
    }

    if (doc.containsKey("timers")) {
      device.timers.clear();
      JsonArray timers = doc["timers"];
      for (JsonObject timerObj : timers) {
        Timer timer;
        if (timerObj.containsKey("isUseSetting")) {
          timer.isUseSetting = timerObj["isUseSetting"].as<bool>();
        }
        if (timerObj.containsKey("time")) {
          strncpy_safe(timer.time, timerObj["time"], MAX_TIME_LENGTH);
        }
        if (timerObj.containsKey("collectionSettings")) {
          JsonArray collectionSettings = timerObj["collectionSettings"];
          for (int i = 0; i < 4 && i < collectionSettings.size(); i++) {
            timer.collectionSettings.set(i, collectionSettings[i].as<bool>());
          }
        }
        if (timerObj.containsKey("initialStateRelay")) {
          JsonObject initialStateRelay = timerObj["initialStateRelay"];
          if (initialStateRelay.containsKey("isUseSetting")) {
            timer.initialStateRelay.isUseSetting = initialStateRelay["isUseSetting"].as<bool>();
          }
          if (initialStateRelay.containsKey("relayId")) {
            timer.initialStateRelay.relayId = initialStateRelay["relayId"];
          }
          if (initialStateRelay.containsKey("statePin")) {
            timer.initialStateRelay.statePin = initialStateRelay["statePin"].as<bool>();
          }
          if (initialStateRelay.containsKey("lastState")) {
            timer.initialStateRelay.lastState = initialStateRelay["lastState"].as<bool>();
          }
          if (initialStateRelay.containsKey("description")) {
            strncpy_safe(timer.initialStateRelay.description, initialStateRelay["description"], MAX_DESCRIPTION_LENGTH);
          }
        }
        if (timerObj.containsKey("endStateRelay")) {
          JsonObject endStateRelay = timerObj["endStateRelay"];
          if (endStateRelay.containsKey("isUseSetting")) {
            timer.endStateRelay.isUseSetting = endStateRelay["isUseSetting"].as<bool>();
          }
          if (endStateRelay.containsKey("relayId")) {
            timer.endStateRelay.relayId = endStateRelay["relayId"];
          }
          if (endStateRelay.containsKey("statePin")) {
            timer.endStateRelay.statePin = endStateRelay["statePin"].as<bool>();
          }
          if (endStateRelay.containsKey("lastState")) {
            timer.endStateRelay.lastState = endStateRelay["lastState"].as<bool>();
          }
          if (endStateRelay.containsKey("description")) {
            strncpy_safe(timer.endStateRelay.description, endStateRelay["description"], MAX_DESCRIPTION_LENGTH);
          }
        }
        device.timers.push_back(timer);
      }
    }

    if (doc.containsKey("isTimersEnabled")) {
      device.isTimersEnabled = doc["isTimersEnabled"].as<bool>();
    }
    if (doc.containsKey("isEncyclateTimers")) {
      device.isEncyclateTimers = doc["isEncyclateTimers"].as<bool>();
    }
    if (doc.containsKey("isScheduleEnabled")) {
      device.isScheduleEnabled = doc["isScheduleEnabled"].as<bool>();
    }
    if (doc.containsKey("isActionEnabled")) {
      device.isActionEnabled = doc["isActionEnabled"].as<bool>();
    }

    return true;
  }

  bool DeviceManager::deserializeDevice(const char* jsonString, Device& device) {

    DynamicJsonDocument doc(8192);

    DeserializationError error = deserializeJson(doc, jsonString);
    if (error) {
      Serial.print("Deserialization error: ");
      Serial.println(error.c_str());
      return false;
    }

    JsonObject json = doc.as<JsonObject>();
    return deserializeDevice(json, device);
  }

  void DeviceManager::setRelayStateForAllDevices(uint8_t targetRelayId, bool state) {
    for (auto& device : myDevices) {

      Relay* relay = findRelayById(device, targetRelayId);
      if (relay) {
        relay->statePin = state;
      }

      if (device.temperature.isUseSetting && device.temperature.relayId == targetRelayId) {

        if (relay) {
          relay->statePin = state;
        }
      }

      for (auto& timer : device.timers) {
        if (timer.isUseSetting) {
          if (timer.initialStateRelay.relayId == targetRelayId) {
            timer.initialStateRelay.statePin = state;
          }
          if (timer.endStateRelay.relayId == targetRelayId) {
            timer.endStateRelay.statePin = state;
          }
        }
      }

      for (auto& scenario : device.scheduleScenarios) {
        if (scenario.isUseSetting) {
          if (scenario.initialStateRelay.relayId == targetRelayId) {
            scenario.initialStateRelay.statePin = state;
          }
          if (scenario.endStateRelay.relayId == targetRelayId) {
            scenario.endStateRelay.statePin = state;
          }
        }
      }
    }
  }

  void DeviceManager::saveRelayStates(uint8_t targetRelayId) {
    for (auto& device : myDevices) {

      Relay* relay = findRelayById(device, targetRelayId);
      if (relay) {
        relay->lastState = relay->statePin;
      }

      if (device.temperature.relayId == targetRelayId) {
        if (relay) {
          device.temperature.lastState = relay->statePin;
        }
      }

      for (auto& timer : device.timers) {
        if (timer.initialStateRelay.relayId == targetRelayId) {
          timer.initialStateRelay.lastState = relay ? relay->statePin : false;
        }
        if (timer.endStateRelay.relayId == targetRelayId) {
          timer.endStateRelay.lastState = relay ? relay->statePin : false;
        }
      }

      for (auto& scenario : device.scheduleScenarios) {
        if (scenario.initialStateRelay.relayId == targetRelayId) {
          scenario.initialStateRelay.lastState = relay ? relay->statePin : false;
        }
        if (scenario.endStateRelay.relayId == targetRelayId) {
          scenario.endStateRelay.lastState = relay ? relay->statePin : false;
        }
      }
    }
  }

  void DeviceManager::restoreRelayStates(uint8_t targetRelayId) {
    for (auto& device : myDevices) {

      Relay* relay = findRelayById(device, targetRelayId);
      if (relay) {
        relay->statePin = relay->lastState;
      }

      if (device.temperature.relayId == targetRelayId) {
        if (relay) {
          relay->statePin = device.temperature.lastState;
        }
      }

      for (auto& timer : device.timers) {
        if (timer.initialStateRelay.relayId == targetRelayId) {
          if (relay) {
            relay->statePin = timer.initialStateRelay.lastState;
          }
        }
        if (timer.endStateRelay.relayId == targetRelayId) {
          if (relay) {
            relay->statePin = timer.endStateRelay.lastState;
          }
        }
      }

      for (auto& scenario : device.scheduleScenarios) {
        if (scenario.initialStateRelay.relayId == targetRelayId) {
          if (relay) {
            relay->statePin = scenario.initialStateRelay.lastState;
          }
        }
        if (scenario.endStateRelay.relayId == targetRelayId) {
          if (relay) {
            relay->statePin = scenario.endStateRelay.lastState;
          }
        }
      }
    }
  }

  Relay* DeviceManager::findRelayById(Device& device, uint8_t relayId) {
    for (auto& relay : device.relays) {
      if (relay.id == relayId) {
        return &relay;
      }
    }
    return nullptr;
  }

  int DeviceManager::findRelayIndexById(const Device& device, uint8_t relayId) {
    for (size_t i = 0; i < device.relays.size(); i++) {
      if (device.relays[i].id == relayId) {
        return i;
      }
    }
    return -1;
  }

  int DeviceManager::findSensorIndexById(const Device& device, int sensorId) {
    for (size_t i = 0; i < device.sensors.size(); i++) {
      if (device.sensors[i].sensorId == sensorId) {
        return i;
      }
    }
    return -1;
  }

  void DeviceManager::validateAndSetRelayId(uint8_t& relayId, const std::vector<Relay>& relays) {
    bool found = false;
    for (const auto& relay : relays) {
      if (relay.id == relayId) {
        found = true;
        break;
      }
    }
    if (!found && !relays.empty()) {
      relayId = relays[0].id;
    }
  }

  void DeviceManager::validateRelayIds(Device& device) {
    validateAndSetRelayId(device.temperature.relayId, device.relays);

    for (auto& scenario : device.scheduleScenarios) {
      validateAndSetRelayId(scenario.initialStateRelay.relayId, device.relays);
      validateAndSetRelayId(scenario.endStateRelay.relayId, device.relays);
    }

    for (auto& timer : device.timers) {
      validateAndSetRelayId(timer.initialStateRelay.relayId, device.relays);
      validateAndSetRelayId(timer.endStateRelay.relayId, device.relays);
    }
  }

  bool DeviceManager::writeDevicesToFile(const std::vector<Device>& myDevices, const char* filename) {
    isSaveControl = true;
    File file = SPIFFS.open(filename, "w");
    if (!file) {
      Serial.println("Ошибка открытия файла для записи");
      isSaveControl = false;
      return false;
    }

    for (const auto& device : myDevices) {
      String json = serializeDevice(device);
      file.println(json);
      yield();
    }

    file.close();
    isSaveControl = false;
    return true;
  }

  bool DeviceManager::readDevicesFromFile(std::vector<Device>& myDevices, const char* filename) {
    Serial.println("readDevicesFromFile");
    Serial.printf("Free heap before: %d\n", ESP.getFreeHeap());

    File file = SPIFFS.open(filename, "r");
    if (!file) {
      Serial.println("Ошибка открытия файла для чтения");
      return false;
    }

    if (file.size() <= 2) {
      Serial.printf("File too small: %d bytes\n", file.size());
      file.close();
      return false;
    }

    myDevices.clear();
    size_t lineCount = 0;
    size_t errorCount = 0;

    while (file.available()) {
      lineCount++;
      String json = file.readStringUntil('\n');
      json.trim();

      if (json.length() == 0) {
        continue;
      }

      Device device;
      if (deserializeDevice(json.c_str(), device)) {
        myDevices.push_back(device);
        Serial.printf("Line %d: Device deserialized successfully\n", lineCount);
      } else {
        errorCount++;
        Serial.printf("Line %d: Deserialization failed: %s\n", lineCount, json.c_str());
      }

      yield();

      if (lineCount % 5 == 0) {
        Serial.printf("Free heap after line %d: %d\n", lineCount, ESP.getFreeHeap());
      }
    }

    file.close();

    Serial.printf("Total lines: %d, errors: %d, devices loaded: %d\n",
                  lineCount, errorCount, myDevices.size());
    Serial.printf("Free heap after: %d\n", ESP.getFreeHeap());

    return errorCount == 0;
  }

  int DeviceManager::getSelectedDeviceIndex(const std::vector<Device>& myDevices) {
    for (size_t i = 0; i < myDevices.size(); ++i) {
      if (myDevices[i].isSelected) {
        return i;
      }
    }
    return -1;
  }

  int DeviceManager::deviceInit() {
    if (!SPIFFS.exists("/devices.json")) {
      initializeDevice("MyDevice1", true);
      writeDevicesToFile(myDevices, "/devices.json");
      Serial.println("Устройство инициализировано и сохранено в файл.");
    } else {
      if (readDevicesFromFile(myDevices, "/devices.json")) {
        return currentDeviceIndex = getSelectedDeviceIndex(myDevices);
      } else {
        Serial.println("Ошибка загрузки устройств из файла.");
        initializeDevice("MyDevice1", true);
        return currentDeviceIndex = 0;
      }
    }
    return currentDeviceIndex = 0;
  }

  String DeviceManager::debugInfo() {
    String debugString;

    if (myDevices.empty()) {
      debugString += "Нет устройств для отладки\n";
      return debugString;
    }

    const Device& currentDevice = myDevices[currentDeviceIndex];

    debugString += "\nУстройство: " + String(currentDevice.nameDevice) + "\n";
    debugString += "Выбрано: " + String(currentDevice.isSelected ? "Да" : "Нет") + "\n";

    debugString += "\nРеле:\n";
    for (const auto& relay : currentDevice.relays) {
      debugString += "ID: " + String(relay.id);
      debugString += ", Pin: " + String(relay.pin);
      debugString += ", ManualMode: " + String(relay.manualMode ? "Вкл" : "Выкл");
      debugString += ", Состояние: " + String(relay.statePin ? "Вкл" : "Выкл");
      debugString += ", Последнее: " + String(relay.lastState ? "Вкл" : "Выкл");
      debugString += ", Описание: " + String(relay.description);
      debugString += "\n";
    }

    debugString += "\nТемпературный контроль:\n";
    debugString += "Активен: " + String(currentDevice.temperature.isUseSetting ? "Да" : "Нет") + "\n";
    if (currentDevice.temperature.isUseSetting) {
      debugString += "Реле ID: " + String(currentDevice.temperature.relayId);
      debugString += ", Сенсор ID: " + String(currentDevice.temperature.sensorId);
      debugString += ", Температура: " + String(currentDevice.temperature.setTemperature);
      debugString += ", Плавно: " + String(currentDevice.temperature.isSmoothly ? "Да" : "Нет");
      debugString += ", Нагрев: " + String(currentDevice.temperature.isIncrease ? "Да" : "Нет");
      debugString += ", PID: " + String(currentDevice.temperature.selectedPidIndex) + "\n";
    }

    debugString += "\nТаймеры (" + String(currentDevice.timers.size()) + "):\n";
    for (size_t i = 0; i < currentDevice.timers.size(); i++) {
      const auto& timer = currentDevice.timers[i];
      debugString += "Таймер #" + String(i + 1) + ": ";
      debugString += "Активен: " + String(timer.isUseSetting ? "Да" : "Нет");
      debugString += ", Время: " + String(timer.time) + "\n";
    }

    debugString += "\nСценарии (" + String(currentDevice.scheduleScenarios.size()) + "):\n";
    for (size_t i = 0; i < currentDevice.scheduleScenarios.size(); i++) {
      const auto& scenario = currentDevice.scheduleScenarios[i];
      debugString += "Сценарий #" + String(i + 1) + ": ";
      debugString += "Активен: " + String(scenario.isUseSetting ? "Да" : "Нет");
      debugString += ", Дата: " + String(scenario.startDate) + " - " + String(scenario.endDate) + "\n";

      for (size_t j = 0; j < scenario.startEndTimes.size(); j++) {
        const auto& interval = scenario.startEndTimes[j];
        debugString += "  Интервал #" + String(j + 1) + ": " + String(interval.startTime) + " - " + String(interval.endTime) + "\n";
      }

      debugString += "  Дни недели: ";
      const char* days[] = {"Пн", "Вт", "Ср", "Чт", "Пт", "Сб", "Вс"};
      for (int d = 0; d < 7; d++) {
        if (scenario.week.get(d)) {
          debugString += String(days[d]) + " ";
        }
      }
      debugString += "\n";

      debugString += "  Нач. состояние: " + String(scenario.initialStateRelay.description);
      debugString += " (Реле ID " + String(scenario.initialStateRelay.relayId);
      debugString += ", " + String(scenario.initialStateRelay.statePin ? "Вкл" : "Выкл") + ")\n";

      debugString += "  Кон. состояние: " + String(scenario.endStateRelay.description);
      debugString += " (Реле ID " + String(scenario.endStateRelay.relayId);
      debugString += ", " + String(scenario.endStateRelay.statePin ? "Вкл" : "Выкл") + ")\n";
    }

    debugString += "\nPID контроллеры (" + String(currentDevice.pids.size()) + "):\n";
    for (size_t i = 0; i < currentDevice.pids.size(); i++) {
      const auto& pid = currentDevice.pids[i];
      debugString += "PID #" + String(i + 1) + ": ";
      debugString += String(pid.description) + " (Kp=" + String(pid.Kp, 2);
      debugString += ", Ki=" + String(pid.Ki, 2);
      debugString += ", Kd=" + String(pid.Kd, 2) + ")\n";
    }

    debugString += "\nСтатус: ";
    debugString += "Таймеры " + String(currentDevice.isTimersEnabled ? "вкл" : "выкл");
    debugString += ", Цикличность " + String(currentDevice.isEncyclateTimers ? "вкл" : "выкл");
    debugString += ", Расписание " + String(currentDevice.isScheduleEnabled ? "вкл" : "выкл") + "\n";

    if (Serial.available() > 0) {
      Serial.println(debugString);
    }

    return debugString;
  }

  size_t DeviceManager::formatFullSystemStatus(char* buffer, size_t bufferSize) {
    if (myDevices.empty() || currentDeviceIndex >= myDevices.size()) {
      return snprintf(buffer, bufferSize, "Нет устройств для отображения статуса\n");
    }

    const Device& device = myDevices[currentDeviceIndex];
    size_t offset = 0;

    offset += snprintf(buffer + offset, bufferSize - offset, "📡 **Датчики:**\n");
    if (device.sensors.empty()) {
      offset += snprintf(buffer + offset, bufferSize - offset, "  Нет настроенных датчиков\n\n");
    } else {
      bool hasActiveSensors = false;
      for (size_t i = 0; i < device.sensors.size(); ++i) {
        const Sensor& sensor = device.sensors[i];
        if (!sensor.isUseSetting) continue;

        hasActiveSensors = true;
        offset += snprintf(buffer + offset, bufferSize - offset,
                           "  • Сенсор #%d (ID: %d): ", i + 1, sensor.sensorId);

        const char* sensorType = "Неизвестный";
        if (sensor.typeSensor.get(0)) sensorType = "DHT11";
        else if (sensor.typeSensor.get(1)) sensorType = "DHT22";
        else if (sensor.typeSensor.get(2)) sensorType = "NTC";
        else if (sensor.typeSensor.get(3)) sensorType = "Touch";
        else if (sensor.typeSensor.get(4)) sensorType = "Аналоговый";

        offset += snprintf(buffer + offset, bufferSize - offset, "%s [Активен]", sensorType);

        bool isSensorOk = true;
        if (sensor.typeSensor.get(0) || sensor.typeSensor.get(1)) {
          if (isnan(sensor.currentValue) || sensor.currentValue < -100.0 || isnan(sensor.humidityValue)) {
            isSensorOk = false;
          }
        }

        if (isSensorOk) {
          if (sensor.typeSensor.get(0) || sensor.typeSensor.get(1) || sensor.typeSensor.get(2)) {
            offset += snprintf(buffer + offset, bufferSize - offset,
                               " - Значение: %.2f°C", (double)sensor.currentValue);
            if (sensor.typeSensor.get(0) || sensor.typeSensor.get(1)) {
              offset += snprintf(buffer + offset, bufferSize - offset,
                                 ", Влажность: %.1f%%", (double)sensor.humidityValue);
            }
          } else if (sensor.typeSensor.get(3)) {
            offset += snprintf(buffer + offset, bufferSize - offset,
                               " - Состояние: %s", sensor.currentValue > 0.5f ? "Нажато" : "Отпущено");
          } else if (sensor.typeSensor.get(4)) {
            offset += snprintf(buffer + offset, bufferSize - offset,
                               " - Значение: %.0f", (double)sensor.currentValue);
          }
        } else {
          offset += snprintf(buffer + offset, bufferSize - offset, " - Датчик не подключен");
        }
        offset += snprintf(buffer + offset, bufferSize - offset, "\n");
      }
      if (!hasActiveSensors) {
        offset += snprintf(buffer + offset, bufferSize - offset, "  Нет активных датчиков\n");
      }
      offset += snprintf(buffer + offset, bufferSize - offset, "\n");
    }

    if (device.temperature.isUseSetting) {
      offset += snprintf(buffer + offset, bufferSize - offset, "🌡️ **Температурный контроль:**\n");
      offset += snprintf(buffer + offset, bufferSize - offset, "  • Статус: [Активен]\n");

      const Sensor* tempSensor = nullptr;
      for (const auto& sensor : device.sensors) {
        if (sensor.sensorId == device.temperature.sensorId) {
          tempSensor = &sensor;
          break;
        }
      }

      if (tempSensor) {
        offset += snprintf(buffer + offset, bufferSize - offset,
                           "  • Текущая температура: %.2f°C\n", (double)tempSensor->currentValue);

        if (tempSensor->typeSensor.get(0) || tempSensor->typeSensor.get(1)) {
          offset += snprintf(buffer + offset, bufferSize - offset,
                             "  • Влажность: %.2f%%\n", (double)tempSensor->humidityValue);
        }
      } else {
        offset += snprintf(buffer + offset, bufferSize - offset,
                           "  • Датчик температуры не найден (ID: %d)\n", device.temperature.sensorId);
      }

      offset += snprintf(buffer + offset, bufferSize - offset,
                         "  • Установленная температура: %.2f°C\n", (double)device.temperature.setTemperature);
      offset += snprintf(buffer + offset, bufferSize - offset,
                         "  • Направление: %s\n", device.temperature.isIncrease ? "Нагрев" : "Охлаждение");
      offset += snprintf(buffer + offset, bufferSize - offset,
                         "  • Режим: %s\n", device.temperature.isSmoothly ? "Плавный (ШИМ)" : "Релейный");

      if (device.temperature.selectedPidIndex < device.pids.size()) {
        const Pid& pid = device.pids[device.temperature.selectedPidIndex];
        offset += snprintf(buffer + offset, bufferSize - offset,
                           "  • PID коэффициенты: Kp=%.2f, Ki=%.2f, Kd=%.2f\n", pid.Kp, pid.Ki, pid.Kd);
      }

      const Relay* tempRelay = nullptr;
      for (const auto& relay : device.relays) {
        if (relay.id == device.temperature.relayId) {
          tempRelay = &relay;
          break;
        }
      }

      if (tempRelay) {
        offset += snprintf(buffer + offset, bufferSize - offset,
                           "  • Управляющее реле: %s (Состояние: %s)\n", tempRelay->description,
                           tempRelay->statePin ? "Вкл" : "Выкл");
      } else {
        offset += snprintf(buffer + offset, bufferSize - offset,
                           "  • Реле управления не найдено (ID: %d)\n", device.temperature.relayId);
      }
      offset += snprintf(buffer + offset, bufferSize - offset, "\n");
    }

    if (device.isScheduleEnabled) {
      offset += snprintf(buffer + offset, bufferSize - offset, "📅 **Расписания:**\n");
      if (device.scheduleScenarios.empty()) {
        offset += snprintf(buffer + offset, bufferSize - offset, "  Нет настроенных расписаний\n\n");
      } else {
        offset += snprintf(buffer + offset, bufferSize - offset, "  • Общий статус: Активен\n\n");

        for (size_t i = 0; i < device.scheduleScenarios.size(); ++i) {
          const ScheduleScenario& scenario = device.scheduleScenarios[i];
          if (!scenario.isUseSetting) continue;

          offset += snprintf(buffer + offset, bufferSize - offset,
                             "  • Сценарий #%d: %s [Активен]", i + 1, scenario.description);

          if (scenario.isActive) {
            offset += snprintf(buffer + offset, bufferSize - offset, " [Выполняется]");
          } else {
            offset += snprintf(buffer + offset, bufferSize - offset, " [Ожидает]");
          }
          offset += snprintf(buffer + offset, bufferSize - offset, "\n");

          String periodText = "    - Период: ";
          periodText += scenario.startDate;
          periodText += " по ";
          periodText += scenario.endDate;
          offset += snprintf(buffer + offset, bufferSize - offset, "%s\n", periodText.c_str());

          offset += snprintf(buffer + offset, bufferSize - offset,
                             "    - Дни: %s\n", getActiveDaysString(scenario.week).c_str());

          offset += snprintf(buffer + offset, bufferSize - offset,
                             "    - Месяцы: %s\n", getActiveMonthsString(scenario.months).c_str());

          if (!scenario.startEndTimes.empty()) {
            offset += snprintf(buffer + offset, bufferSize - offset, "    - Время: ");
            for (size_t j = 0; j < scenario.startEndTimes.size(); ++j) {
              if (j > 0) offset += snprintf(buffer + offset, bufferSize - offset, ", ");
              String timeText = scenario.startEndTimes[j].startTime;
              timeText += "-";
              timeText += scenario.startEndTimes[j].endTime;
              offset += snprintf(buffer + offset, bufferSize - offset, "%s", timeText.c_str());
            }
            offset += snprintf(buffer + offset, bufferSize - offset, "\n");
          }

          offset += snprintf(buffer + offset, bufferSize - offset,
                             "    - Действия: %s%s%s\n",
                             scenario.collectionSettings.get(0) ? "Температура " : "",
                             scenario.collectionSettings.get(1) ? "Таймеры " : "",
                             scenario.collectionSettings.get(2) ? "Реле " : "");

          offset += snprintf(buffer + offset, bufferSize - offset, "\n");
        }
        offset += snprintf(buffer + offset, bufferSize - offset, "\n");
      }
    }

    if (device.isActionEnabled) {

      offset += snprintf(buffer + offset, bufferSize - offset, "⚙️ **Действия по датчикам:**\n");

      if (device.actions.empty()) {
        offset += snprintf(buffer + offset, bufferSize - offset, "  Нет настроенных действий\n\n");
      } else {
        offset += snprintf(buffer + offset, bufferSize - offset, "  • Общий статус: Активен\n\n");

        for (size_t i = 0; i < device.actions.size(); ++i) {
          const Action& action = device.actions[i];
          if (!action.isUseSetting) continue;

          offset += snprintf(buffer + offset, bufferSize - offset,
                             "  • Действие #%d: %s [Активен]", i + 1, action.description);

          if (action.wasTriggered) {
            offset += snprintf(buffer + offset, bufferSize - offset, " [Сработало]");
          } else {
            offset += snprintf(buffer + offset, bufferSize - offset, " [Ожидает]");
          }
          offset += snprintf(buffer + offset, bufferSize - offset, "\n");

          const Sensor* targetSensor = nullptr;
          for (const auto& sensor : device.sensors) {
            if (sensor.sensorId == action.targetSensorId) {
              targetSensor = &sensor;
              break;
            }
          }

          if (targetSensor) {

            offset += snprintf(buffer + offset, bufferSize - offset,
                               "  Датчик: %s (ID: %d)\n", targetSensor->description, targetSensor->sensorId);

            float currentValue = action.isHumidity ? targetSensor->humidityValue : targetSensor->currentValue;
            if (!isnan(currentValue)) {
              offset += snprintf(buffer + offset, bufferSize - offset,
                                 "  Текущее значение: %.2f\n", (double)currentValue);
            } else {
              offset += snprintf(buffer + offset, bufferSize - offset,
                                 "  Текущее значение: Н/Д\n");
            }
          } else {
            offset += snprintf(buffer + offset, bufferSize - offset,
                               "  Датчик не найден (ID: %d)\n", action.targetSensorId);
          }

          offset += snprintf(buffer + offset, bufferSize - offset, "  Пороги: ");

          if (!isnan(action.triggerValueMax)) {
            offset += snprintf(buffer + offset, bufferSize - offset, "Срабатывание %.2f", (double)action.triggerValueMax);
          } else {
            offset += snprintf(buffer + offset, bufferSize - offset, "Срабатывание N/A");
          }

          offset += snprintf(buffer + offset, bufferSize - offset, " / ");

          if (!isnan(action.triggerValueMin)) {
            offset += snprintf(buffer + offset, bufferSize - offset, "Сброс %.2f\n", (double)action.triggerValueMin);
          } else {
            offset += snprintf(buffer + offset, bufferSize - offset, "Сброс N/A\n");
          }

          String actionTypes = "";
          if (action.collectionSettings.get(0)) actionTypes += "Таймеры ";
          if (action.collectionSettings.get(1)) actionTypes += "Реле ";
          if (action.collectionSettings.get(2)) actionTypes += "Температура ";
          if (action.collectionSettings.get(3)) actionTypes += "Сообщение";

          offset += snprintf(buffer + offset, bufferSize - offset,
                             "    - Тип действия: %s\n", actionTypes.c_str());

          if (action.collectionSettings.get(3) && action.sendMsg.length() > 0) {
            offset += snprintf(buffer + offset, bufferSize - offset,
                               "    - Сообщение: %s\n", action.sendMsg.c_str());
          }

          if (!action.collectionSettings.get(3)) {
            offset += snprintf(buffer + offset, bufferSize - offset,
                               "    - Возврат: %s\n", action.isReturnSetting ? "Включен" : "Выключен");
          }

          offset += snprintf(buffer + offset, bufferSize - offset, "\n");
        }
        offset += snprintf(buffer + offset, bufferSize - offset, "\n");
      }
    }

    if (device.isTimersEnabled && !device.timers.empty()) {
      offset += snprintf(buffer + offset, bufferSize - offset, "⏱️ **Таймеры:**\n");
      offset += snprintf(buffer + offset, bufferSize - offset,
                         "  • Общий статус: %s\n", device.isTimersEnabled ? "Активен" : "Неактивен");
      offset += snprintf(buffer + offset, bufferSize - offset,
                         "  • Циклическое выполнение: %s\n\n", device.isEncyclateTimers ? "Включено" : "Выключено");

      for (size_t i = 0; i < device.timers.size(); ++i) {
        const Timer& timer = device.timers[i];
        if (!timer.isUseSetting) continue;

        offset += snprintf(buffer + offset, bufferSize - offset,
                           "  • Таймер #%d: %s\n", i + 1, timer.time);

        if (timer.progress.isRunning) {
          offset += snprintf(buffer + offset, bufferSize - offset,
                             "  - Статус: Выполняется\n");
          offset += snprintf(buffer + offset, bufferSize - offset,
                             "  - Прошло: %d сек\n", timer.progress.elapsedTime / 1000);
          offset += snprintf(buffer + offset, bufferSize - offset,
                             "  - Осталось: %d сек\n", timer.progress.remainingTime / 1000);
        } else if (timer.progress.isStopped) {
          offset += snprintf(buffer + offset, bufferSize - offset, "  - Статус: Остановлен\n");
        } else {
          offset += snprintf(buffer + offset, bufferSize - offset, "  - Статус: Ожидает\n");
        }

        offset += snprintf(buffer + offset, bufferSize - offset,
                           "  - Действие: %s%s%s\n",
                           timer.collectionSettings.get(0) ? "Реле " : "",
                           timer.collectionSettings.get(1) ? "Температура " : "",
                           timer.collectionSettings.get(2) ? "Без действия " : "");

        offset += snprintf(buffer + offset, bufferSize - offset, "\n");
      }
      offset += snprintf(buffer + offset, bufferSize - offset, "\n");
    }
    return offset;
  }

  String DeviceManager::getActiveDaysString(const BitArray7& week) {
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

  String DeviceManager::getActiveMonthsString(const BitArray12& months) {
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

  void DeviceManager::printDevices(const std::vector<Device>& devices) {
    if (devices.empty()) {
      Serial.println("Устройства не найдены.");
      return;
    }

    bool foundSelected = false;

    for (size_t i = 0; i < devices.size(); ++i) {
      const auto& device = devices[i];
      Serial.println("Устройство #" + String(i));
      Serial.println("  Имя: " + String(device.nameDevice));
      Serial.println("  Выбрано: " + String(device.isSelected ? "Да" : "Нет"));
      Serial.println("  Количество реле: " + String(device.relays.size()));
      Serial.println("  Количество PID: " + String(device.pids.size()));
      Serial.println("  Количество таймеров: " + String(device.timers.size()));

      Serial.println("-----------------------------");

      if (device.isSelected) {
        Serial.println("Выбранное устройство имеет индекс: " + String(i));
        foundSelected = true;
      }
    }

    if (!foundSelected) {
      Serial.println("Нет выбранного устройства.");
    }
  }

  bool DeviceManager::checkSettingsChanged() {

    static struct {

      bool timersEnabled;
      bool scheduleEnabled;
      bool tempEnabled;
      bool actionEnabled;
      bool encyclate;

      size_t relaysCount;
      size_t sensorsCount;
      size_t timersCount;
      size_t actionsCount;
      size_t scenariosCount;
      size_t pidsCount;
    } lastState = {false, false, false, false, false, 0, 0, 0, 0, 0, 0};

    Device& device = myDevices[currentDeviceIndex];

    bool flagsChanged = (device.isTimersEnabled != lastState.timersEnabled) ||
                        (device.isScheduleEnabled != lastState.scheduleEnabled) ||
                        (device.temperature.isUseSetting != lastState.tempEnabled) ||
                        (device.isActionEnabled != lastState.actionEnabled) ||
                        (device.isEncyclateTimers != lastState.encyclate);

    bool countsChanged = (device.relays.size() != lastState.relaysCount) ||
                         (device.sensors.size() != lastState.sensorsCount) ||
                         (device.timers.size() != lastState.timersCount) ||
                         (device.actions.size() != lastState.actionsCount) ||
                         (device.scheduleScenarios.size() != lastState.scenariosCount) ||
                         (device.pids.size() != lastState.pidsCount);

    bool anyChanged = flagsChanged || countsChanged;

    if (anyChanged) {
      Serial.println(">>> КРИТИЧЕСКИЕ ИЗМЕНЕНИЯ НАСТРОЕК ОБНАРУЖЕНЫ <<<");

      lastState.timersEnabled = device.isTimersEnabled;
      lastState.scheduleEnabled = device.isScheduleEnabled;
      lastState.tempEnabled = device.temperature.isUseSetting;
      lastState.actionEnabled = device.isActionEnabled;
      lastState.encyclate = device.isEncyclateTimers;

      lastState.relaysCount = device.relays.size();
      lastState.sensorsCount = device.sensors.size();
      lastState.timersCount = device.timers.size();
      lastState.actionsCount = device.actions.size();
      lastState.scenariosCount = device.scheduleScenarios.size();
      lastState.pidsCount = device.pids.size();
    }

    return anyChanged;
  }

  uint32_t DeviceManager::calculateOutputRelayChecksum() {
    Device& device = myDevices[currentDeviceIndex];
    uint32_t hash = 5381;

    auto addToHash = [&hash](const void* data, size_t size) {
      const uint8_t* bytes = static_cast<const uint8_t*>(data);
      for (size_t i = 0; i < size; ++i) {
        hash = ((hash << 5) + hash) + bytes[i];
      }
    };

    for (const auto& relay : device.relays) {

      if (relay.isOutput) {

        addToHash(&relay.id, sizeof(relay.id));
        addToHash(&relay.pin, sizeof(relay.pin));
        addToHash(&relay.isOutput, sizeof(relay.isOutput));
        addToHash(&relay.isPwm, sizeof(relay.isPwm));
        addToHash(&relay.pwm, sizeof(relay.pwm));
        addToHash(&relay.manualMode, sizeof(relay.manualMode));
        addToHash(relay.description, strnlen(relay.description, MAX_DESCRIPTION_LENGTH));

        bool state = relay.statePin;
        addToHash(&state, sizeof(state));
      }
    }
    return hash;
  }

  uint32_t DeviceManager::calculateSensorValuesChecksum() {
    Device& device = myDevices[currentDeviceIndex];
    uint32_t hash = 5381;

    auto addToHash = [&hash](const void* data, size_t size) {
      const uint8_t* bytes = static_cast<const uint8_t*>(data);
      for (size_t i = 0; i < size; ++i) {
        hash = ((hash << 5) + hash) + bytes[i];
      }
    };

    for (const auto& sensor : device.sensors) {

      if (sensor.isUseSetting) {

        addToHash(&sensor.currentValue, sizeof(sensor.currentValue));

        addToHash(&sensor.humidityValue, sizeof(sensor.humidityValue));
      }
    }
    return hash;
  }

  bool DeviceManager::relayStateChanged() {

    if (millis() - lastRelayCheckTime < RELAY_CHECK_INTERVAL) {
      return false;
    }
    lastRelayCheckTime = millis();

    uint32_t currentChecksum = calculateOutputRelayChecksum();

    if (currentChecksum != lastOutputRelayChecksum) {

      lastOutputRelayChecksum = currentChecksum;

      return true;
    }

    return false;
  }

  String DeviceManager::serializeRelaysForControlTab() {
    if (myDevices.empty() || currentDeviceIndex >= myDevices.size()) {

      return "{\"type\":\"relays_update\",\"relays\":[]}";
    }

    const Device& device = myDevices[currentDeviceIndex];

    DynamicJsonDocument doc(2048);

    doc["type"] = "relays_update";

    JsonArray relaysArray = doc.createNestedArray("relays");

    for (const auto& relay : device.relays) {

      if (relay.isOutput) {

        JsonObject relayObj = relaysArray.createNestedObject();

        relayObj["description"] = relay.description;
        relayObj["statePin"] = relay.statePin;
        relayObj["id"] = relay.id;
        relayObj["manualMode"] = relay.manualMode;
      }
    }

    String jsonString;
    serializeJson(doc, jsonString);

    return jsonString;
  }

  bool DeviceManager::handleRelayCommand(const JsonObject& command, uint32_t clientNum) {

    if (myDevices.empty()) {
      Serial.println("[DeviceManager] Error: No devices configured.");
      return false;
    }

    Serial.println("[DeviceManager] Received relay command.");

    Device& device = myDevices[currentDeviceIndex];

    int relayId = command["relay"];
    const char* action = command["action"];

    if (!action) {
      Serial.println("[DeviceManager] Error: 'action' field missing in command.");
      return false;
    }

    if (strcmp(action, "reset_all") == 0) {
      Serial.println("[DeviceManager] Executing 'reset_all' command for all relays.");
      bool anyRelayFound = false;
      for (auto& relay : device.relays) {
        if (relay.isOutput) {
          relay.statePin = false;
          relay.manualMode = false;
          anyRelayFound = true;
        }
      }
      return anyRelayFound;
    }

    if (command["relay"].isNull()) {
      Serial.println("[DeviceManager] Error: 'relay' field (ID) missing for this command.");
      return false;
    }

    bool found = false;

    Serial.printf("[DeviceManager] Searching for relay with ID: %d\n", relayId);
    for (auto& relay : device.relays) {
      if (relay.id == relayId) {

        found = true;
        Serial.printf("[DeviceManager] Found relay '%s' (ID: %d). Executing command '%s'\n", relay.description, relay.id, action);

        if (strcmp(action, "reset") == 0) {
          relay.manualMode = false;
          Serial.printf("[DeviceManager] Relay ID %d set to Auto mode.\n", relay.id);
        }

        else if (strcmp(action, "on") == 0 || strcmp(action, "off") == 0) {
          bool newState = (strcmp(action, "on") == 0);

          if (relay.statePin != newState || !relay.manualMode) {
            relay.statePin = newState;
            relay.manualMode = true;

            Serial.printf("[DeviceManager] Relay ID %d state set to '%s' and mode to Manual.\n", relay.id,  relay.statePin ? "ON" : "OFF");
          } else {
            Serial.printf("[DeviceManager] Relay ID %d already in desired state '%s'. No action taken.\n", relay.id, newState ? "ON" : "OFF");
          }
        }
        else {
          Serial.printf("[DeviceManager] Error: Unknown action '%s' for relay ID %d.\n", action, relayId);
          found = false;
        }
        break;
      }
    }

    if (!found) {
      Serial.printf("[DeviceManager] Error: Relay with ID %d not found.\n", relayId);
    }

    return found;
  }

  String DeviceManager::serializeTimersProgress() {
    if (myDevices.empty() || currentDeviceIndex >= myDevices.size()) {

      return "{\"type\":\"timers_update\",\"timers\":[]}";
    }

    const Device& device = myDevices[currentDeviceIndex];

    DynamicJsonDocument doc(2048);

    doc["type"] = "timers_update";

    JsonArray timersJson = doc.createNestedArray("timers");

    for (size_t i = 0; i < device.timers.size(); ++i) {
      const Timer& timer = device.timers[i];
      const TimerInfo& progress = timer.progress;

      JsonObject timerObj = timersJson.createNestedObject();
      timerObj["i"] = i;
      timerObj["e"] = timer.isUseSetting;
      timerObj["et"] = progress.elapsedTime;
      timerObj["rt"] = progress.remainingTime;
      timerObj["r"] = progress.isRunning;
      timerObj["s"] = progress.isStopped;
    }

    String jsonString;
    serializeJson(doc, jsonString);
    return jsonString;
  }

  String DeviceManager::serializeDeviceFlags() {
    if (myDevices.empty() || currentDeviceIndex >= myDevices.size()) {

      return "{\"type\":\"device_flags_update\"}";
    }

    const Device& device = myDevices[currentDeviceIndex];

    DynamicJsonDocument doc(512);

    doc["type"] = "device_flags_update";

    doc["name"] = device.nameDevice;
    doc["sel"] = device.isSelected;
    doc["te"] = device.isTimersEnabled;
    doc["tc"] = device.isEncyclateTimers;
    doc["se"] = device.isScheduleEnabled;
    doc["ae"] = device.isActionEnabled;
    doc["tu"] = device.temperature.isUseSetting;

    String jsonString;
    serializeJson(doc, jsonString);
    return jsonString;
  }

  bool DeviceManager::sensorValuesChanged() {

    if (millis() - lastSensorCheckTime < SENSOR_CHECK_INTERVAL) {
      return false;
    }
    lastSensorCheckTime = millis();

    uint32_t currentChecksum = calculateSensorValuesChecksum();

    if (currentChecksum != lastSensorValuesChecksum) {

      lastSensorValuesChecksum = currentChecksum;

      return true;
    }

    return false;
  }

  uint32_t DeviceManager::calculateTimersProgressChecksum() {
    if (myDevices.empty() || currentDeviceIndex >= myDevices.size()) {
      return 0;
    }
    const Device& device = myDevices[currentDeviceIndex];
    uint32_t hash = 5381;

    auto addToHash = [&hash](const void* data, size_t size) {
      const uint8_t* bytes = static_cast<const uint8_t*>(data);
      for (size_t i = 0; i < size; ++i) {
        hash = ((hash << 5) + hash) + bytes[i];
      }
    };

    for (const auto& timer : device.timers) {
      if (timer.isUseSetting) {

        addToHash(&timer.progress.elapsedTime, sizeof(timer.progress.elapsedTime));
        addToHash(&timer.progress.remainingTime, sizeof(timer.progress.remainingTime));
        addToHash(&timer.progress.isRunning, sizeof(timer.progress.isRunning));
        addToHash(&timer.progress.isStopped, sizeof(timer.progress.isStopped));
      }
    }
    return hash;
  }

  bool DeviceManager::timersProgressChanged() {
    static unsigned long lastCheckTime = 0;
    if (millis() - lastCheckTime < 500) {
      return false;
    }
    lastCheckTime = millis();

    uint32_t currentChecksum = calculateTimersProgressChecksum();

    if (currentChecksum != lastTimersProgressChecksum) {
      Serial.printf(">>> ИЗМЕНЕНИЕ ПРОГРЕССА ТАЙМЕРОВ ОБНАРУЖЕНО! New checksum: %u\n", currentChecksum);
      lastTimersProgressChecksum = currentChecksum;
      return true;
    }

    return false;
  }

  String DeviceManager::serializeSensorValues() {
    if (myDevices.empty() || currentDeviceIndex >= myDevices.size()) {

      return "{\"type\":\"sensor_values_update\",\"sensors\":[]}";
    }

    const Device& device = myDevices[currentDeviceIndex];

    DynamicJsonDocument doc(2048);

    doc["type"] = "sensor_values_update";

    JsonArray sensorsArray = doc.createNestedArray("sensors");

    for (const auto& sensor : device.sensors) {

      if (sensor.isUseSetting) {

        JsonObject sensorObj = sensorsArray.createNestedObject();

        sensorObj["id"] = sensor.sensorId;
        sensorObj["ds"] = sensor.description;
        sensorObj["cv"] = sensor.currentValue;
        sensorObj["hv"] = sensor.humidityValue;
      }
    }

    String jsonString;
    serializeJson(doc, jsonString);

    return jsonString;
  }

  void DeviceManager::showMemoryInfo() {
  #ifdef ESP8266
    Serial.printf("Free Heap: %u\n", ESP.getFreeHeap());
    Serial.printf("Max Free Block: %u\n", ESP.getMaxFreeBlockSize());
    Serial.printf("Heap Fragmentation: %u%%\n", ESP.getHeapFragmentation());
  #elif defined(ESP32)
    Serial.printf("Free Heap: %u\n", ESP.getFreeHeap());
    Serial.printf("Max Free Block: %u\n", ESP.getMaxAllocHeap());
    Serial.printf("Min Free Heap: %u\n", ESP.getMinFreeHeap());
  #endif
  }
