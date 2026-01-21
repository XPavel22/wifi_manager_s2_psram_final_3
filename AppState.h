#pragma once
#include <Arduino.h>

    struct AppState {

    enum ProcessState {
    STATE_IDLE,
    STATE_CONNECTING,
    STATE_READING_HEADERS,
    STATE_DOWNLOADING,
    STATE_COMPLETE,
    STATE_ERROR
};

ProcessState processState;

    enum ConnectState {
        CONNECT_IDLE,
        CONNECT_PREPARING,
        CONNECT_STARTED,
        CONNECT_WAITING,
        CONNECT_COMPLETING
    };

    ConnectState connectState = CONNECT_IDLE;
    bool wifiConnected = false;
    bool isAP = false;
    bool isTemporaryAP = false;
    bool isScanning = false;
    bool isInternetAvailable = false;
    String ipAdress;
    bool isStartWifi = false;

    bool isUpdating = false;
    bool isFormat = false;

};
