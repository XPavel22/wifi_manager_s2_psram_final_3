#pragma once

#include <DHT.h>
#include "CommonTypes.h"

#define MAX_DESCRIPTION_LENGTH 120
#define MAX_TXT_DESCRIPTION_LENGTH 512
#define MAX_TIME_LENGTH 10
#define MAX_DATE_LENGTH 11

struct TouchSensorState {
  unsigned long lastDebounceTime = 0;
  bool lastState = HIGH;
  bool pressed = false;
  unsigned long pressStartTime = 0;
};

struct BitArray4 {
    uint8_t bits;
    bool get(int index) const { return (bits >> index) & 1; }
    void set(int index, bool value) {
        if (value) bits |= (1 << index);
        else bits &= ~(1 << index);
    }
    void clear() { bits = 0; }
};

struct BitArray7 {
    uint8_t bits;
    bool get(int index) const { return (bits >> index) & 1; }
    void set(int index, bool value) {
        if (value) bits |= (1 << index);
        else bits &= ~(1 << index);
    }
    void clear() { bits = 0; }
};

struct BitArray12 {
    uint16_t bits;
    bool get(int index) const { return (bits >> index) & 1; }
    void set(int index, bool value) {
        if (value) bits |= (1 << index);
        else bits &= ~(1 << index);
    }
    void clear() { bits = 0; }
};

struct BitArray2 {
    uint8_t bits;
    bool get(int index) const { return (bits >> index) & 1; }
    void set(int index, bool value) {
        if (value) bits |= (1 << index);
        else bits &= ~(1 << index);
    }
    void clear() { bits = 0; }
};

struct Relay {
  int id;
  uint8_t pin;
  bool manualMode;
  bool isOutput;
  bool isDigital;
  bool statePin;
  bool isPwm;
  uint8_t pwm;
  bool lastState;
  char description[MAX_DESCRIPTION_LENGTH];
};

struct OutPower {
  bool isUseSetting;
  uint8_t relayId;
  bool statePin;
  bool lastState;
  bool isPwm;
  uint8_t pwm;
  bool isReturn;
  char description[MAX_DESCRIPTION_LENGTH];
};

struct startEndTime {
  char startTime[MAX_TIME_LENGTH];
  char endTime[MAX_TIME_LENGTH];
};

struct ScheduleScenario {
  char description[MAX_DESCRIPTION_LENGTH];
  bool isUseSetting;
  bool isActive;
  BitArray4 collectionSettings;
  char startDate[MAX_DATE_LENGTH];
  char endDate[MAX_DATE_LENGTH];
  std::vector<startEndTime> startEndTimes;
  BitArray7 week;
  BitArray12 months;
  OutPower initialStateRelay;
  OutPower endStateRelay;

  bool temperatureUpdated;
  bool timersExecuted;
  bool initialStateApplied;
  bool endStateApplied;
  bool scenarioProcessed;
};

struct Pid {
  char description[MAX_DESCRIPTION_LENGTH];
  char descriptionDetailed[MAX_TXT_DESCRIPTION_LENGTH];
  double Kp;
  double Ki;
  double Kd;
};

struct Sensor {
  bool isUseSetting;
  int sensorId;
  int relayId;
  BitArray7 typeSensor;
  uint16_t serial_r;
  uint16_t thermistor_r;
  float currentValue = 0.0;
  float humidityValue = 0.0;
  DHT* dht = nullptr;
  char description[MAX_DESCRIPTION_LENGTH];
};

struct Action {
   bool isUseSetting;
   char description[MAX_DESCRIPTION_LENGTH];
   int targetSensorId;
   float triggerValueMax;
   float triggerValueMin;
   bool isHumidity;
   bool actionMoreOrEqual;
   std::vector<OutPower> outputs;
   BitArray4 collectionSettings;
   String sendMsg;
   bool isReturnSetting;
   bool wasTriggered;
};

struct Temperature {
    bool isUseSetting;
    uint8_t relayId;
    bool lastState;
    uint8_t sensorId;
    int setTemperature;
    float currentTemp;
    bool isSmoothly;
    bool isIncrease;
    BitArray4 collectionSettings;
    uint8_t selectedPidIndex;
    unsigned long pidOutputMs;

     Sensor* sensorPtr = nullptr;
    Relay* relayPtr = nullptr;
};

struct TimerInfo {
    unsigned long elapsedTime = 0;
    unsigned long remainingTime = 0;
    bool isRunning = false;
    bool isStopped = false;

    void clear() {
        elapsedTime = 0;
        remainingTime = 0;
        isRunning = false;
        isStopped = false;
    }
};

struct Timer {
  bool isUseSetting;
  char time[MAX_TIME_LENGTH];
  BitArray4 collectionSettings;
  OutPower initialStateRelay;
  OutPower endStateRelay;
  TimerInfo progress;
};

struct Device {
  char nameDevice[MAX_DESCRIPTION_LENGTH];
  bool isSelected;
  std::vector<Relay> relays;
  std::vector<uint8_t> pins;
  std::vector<ScheduleScenario> scheduleScenarios;
  Temperature temperature;

  std::vector<Pid> pids;
  std::vector<Timer> timers;
  std::vector<Sensor> sensors;
  std::vector<Action> actions;

  bool isTimersEnabled;
  bool isEncyclateTimers;
  bool isScheduleEnabled;
  bool isActionEnabled;
};

class DeviceManager {
public:
    DeviceManager() = default;

    std::vector<Device> myDevices;
    uint8_t currentDeviceIndex = 0;
    bool isSaveControl = false;
    bool isResultSaveControl = false;

    struct {
        char relayStates[100];
        float lastTemp = -1000;
        char lastStatus[100];
    } lastState;

     size_t formatFullSystemStatus(char* buffer, size_t bufferSize);

    void initializeDevice(const char* name, bool activ, bool isNewDevice = false);
    int deviceInit();

    String serializeDevice(const Device& device);
    bool deserializeDevice(JsonObject doc, Device& device);
    bool deserializeDevice(const char* jsonString, Device& device);
    bool writeDevicesToFile(const std::vector<Device>& myDevices, const char* filename);
    bool readDevicesFromFile(std::vector<Device>& myDevices, const char* filename);

    void setRelayStateForAllDevices(uint8_t targetRelayId, bool state);
    void saveRelayStates(uint8_t targetRelayId);
    void restoreRelayStates(uint8_t targetRelayId);
    void validateAndSetRelayId(uint8_t& relayId, const std::vector<Relay>& relays);
    void validateRelayIds(Device& device);

    int getSelectedDeviceIndex(const std::vector<Device>& myDevices);
    String debugInfo();
    void printDevices(const std::vector<Device>& devices);
    void showMemoryInfo();

    bool checkSettingsChanged();
    bool relayStateChanged();
    bool sensorValuesChanged();
    bool timersProgressChanged();
    bool handleRelayCommand(const JsonObject& command, uint32_t clientNum);
    String serializeRelaysForControlTab();
    String serializeTimersProgress();
    String serializeDeviceFlags();
    String serializeSensorValues();

    String getActiveDaysString(const BitArray7& week);
    String getActiveMonthsString(const BitArray12& months);

private:

    int findRelayIndexById(const Device& device, uint8_t relayId);
    int findSensorIndexById(const Device& device, int sensorId);
    Relay* findRelayById(Device& device, uint8_t relayId);

    void strncpy_safe(char* dest, const char* src, size_t destSize) {
        strncpy(dest, src, destSize - 1);
        dest[destSize - 1] = '\0';
    }

    uint32_t lastOutputRelayChecksum = 0;
    unsigned long lastRelayCheckTime = 0;
    const unsigned long RELAY_CHECK_INTERVAL = 500;

    unsigned long lastSensorCheckTime = 0;
    uint32_t lastSensorValuesChecksum = 0;
    const unsigned long SENSOR_CHECK_INTERVAL = 200;

    uint32_t lastTimersProgressChecksum = 0;
    uint32_t calculateTimersProgressChecksum();

    uint32_t calculateOutputRelayChecksum();
    uint32_t calculateSensorValuesChecksum();

};
