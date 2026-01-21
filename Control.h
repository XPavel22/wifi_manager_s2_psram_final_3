#ifndef CONTROL_H
#define CONTROL_H

#include "CommonTypes.h"

#include <functional>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <PID_v1.h>
#include <DHT.h>
#include "DeviceManager.h"
#include "Logger.h"
#include <time.h>
#include <memory>

#ifdef ESP8266
#include <ESP8266WiFi.h>
#elif defined(ESP32)
#include <WiFi.h>
#endif

class Control {
private:
    Logger& logger;
     DeviceManager& deviceManager;

    std::vector<Device>& myDevices;
    uint8_t& currentDeviceIndex;

    std::unordered_map<uint8_t, TouchSensorState> touchStates;
    std::unique_ptr<PID> myPID;

    unsigned long lastPidTime;
    double outputPid;
    double inputPid;
    double setpointPid;
    const int pidWindowSize = 255;
    unsigned long pidWindowStartTime;

    bool debug = false;

    unsigned long lastUpdate = 0;

enum DhtReadState {
        DHT_IDLE,
        DHT_READING
    };

    DhtReadState dhtReadState = DHT_IDLE;
    Sensor* currentlyReadingDhtSensor = nullptr;
    int nextDhtSensorIndex = 0;

    bool isNumeric(const String& str);
    bool isValidDateTime(const String& dateTime);
    int shiftWeekDay(int currentDay);
    time_t convertToTimeT(const String& date);
    time_t getCurrentTime();
    String formatDateTime(time_t rawTime);
    time_t convertOnlyDate(time_t rawTime);
    uint32_t timeStringToSeconds(const String& timeStr);
    String secondsToTimeString(uint32_t totalSeconds);
    void controlOutputs(OutPower& outPower);
    void adjustPidCoefficients(float gradient, float error);
    void setFlagsSettingsTimers(uint8_t selectedIndex, Timer& currentTimer);
    void collectionSettingsTimer(uint8_t currentTimerIndex);
    void executeTimers(bool state);
    void saveRelayStates(uint8_t relayIndex);
    void restoreRelayStates(uint8_t relayIndex);
    void collectionSettingsSchedule(bool start, ScheduleScenario& scenario);
    String getActiveDaysString(const BitArray7& week);
    String getActiveMonthsString(const BitArray12& months);
    float readNTCTemperature(const Sensor& sensor);
    int readAnalog(const Sensor& sensor);
    void resetActionEffects(Action& action, Device& device);

 struct {
    bool isActive = false;
    bool pidInitialized = false;
  } tempControlState;

    Relay* findRelayById(Device& device, int id);
    Sensor* findSensorById(Device& device, int id);

public:

    Control(DeviceManager& dm, Logger& logger);

    void setup();
    void update();
    void loop();

    void setupControl();
    void readSensors();
    void setTemperature();
    void setTimersExecute();
    void updatePins();
    void setSchedules();
    void setSensorActions();

    void manualWork(String arg);

    String currentStateSensors();
    String currentStatus();
    String sendStatus();
    String sendHelp();
    String getSensorStatus();
    String getSystemStatus();
    String getHelp();

    void processCommand(const String& command);

    bool checkTouchSensor(uint8_t pin);

    bool isDebug() const { return debug; }
};

#endif
