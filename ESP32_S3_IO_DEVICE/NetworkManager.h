#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <WiFi.h>
#include <Preferences.h>
#include "Config.h"

class AppNetworkManager {
public:
    void begin();
    void loop();
    void saveConfig(const WifiConfig& cfg);
    const WifiConfig& getConfig() const { return _config; }
    String getDeviceName() const { return _uniqueName; }
    void setLedStatusMode(bool mode);
    bool isConnected() const;
    static void WiFiEvent(WiFiEvent_t event);

private:
    String _uniqueName;
    WifiConfig _config;
    Preferences _prefs;
    bool _wifiConnected = false;
    unsigned long _reconnectAttemptTime = 0;

    void loadConfig();
    void connect();
    void reconnect();
    void handleWiFiEvent(WiFiEvent_t event);
};

extern AppNetworkManager AppNet;
#endif