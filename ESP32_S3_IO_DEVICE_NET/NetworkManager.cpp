/**
 * @file NetworkManager.cpp
 * @brief WiFi, mDNS, and NVS management implementation.
 * @copyright Copyright (c) 2024 norit. Licensed under the MIT License.
 */
#include "NetworkManager.h"
#include "HardwareManager.h"
#include <ESPmDNS.h>

// Prototype for private local handler
void handleFactoryReset();

// Static member for WiFi event handling
void AppNetworkManager::WiFiEvent(WiFiEvent_t event) {
    AppNet.handleWiFiEvent(event);
}
/**
 * @brief Bootstraps network services.
 * Resets WiFi stack, starts AP, mDNS, and connects to STA if configured.
 */
void AppNetworkManager::begin() {
    loadConfig();
    
    // WiFiドライバ独自のフラッシュ保存機能を無効化する
    // これにより、起動時に古い情報で勝手に接続を試みるのを防ぎ、エラーログを軽減します
    WiFi.persistent(false);
    WiFi.onEvent(AppNetworkManager::WiFiEvent); // Register event handler

    // Generate unique name based on MAC address
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char suffix[7];
    // Use last 3 bytes (24-bit) to minimize collision probability
    sprintf(suffix, "%02X%02X%02X", mac[3], mac[4], mac[5]);
    _uniqueName = String(DEVICE_NAME_BASE) + "_" + suffix;

    // モード設定前に一旦OFFにして状態をクリア
    WiFi.mode(WIFI_OFF);
    delay(100);

    WiFi.mode(WIFI_AP_STA);
    WiFi.setHostname(_uniqueName.c_str());
    WiFi.softAP(_uniqueName.c_str(), DEFAULT_AP_PASS);

    if (MDNS.begin(_uniqueName.c_str())) {
        Serial.println("mDNS responder started");
        MDNS.addService("http", "tcp", 80); // HTTPサービスを登録して見つけやすくする
    }
    connect();
}

void AppNetworkManager::loop() {
    if (_config.ledStatusMode) Hardware.updateStatusLed(isConnected());
    handleFactoryReset();

    // Reconnection logic: Attempt to reconnect if not connected and an SSID is configured
    if (!_wifiConnected && _config.ssid.length() > 0 && (millis() - _reconnectAttemptTime > WIFI_RECONNECT_INTERVAL_MS)) {
        Serial.println("Attempting to reconnect to WiFi...");
        reconnect();
        _reconnectAttemptTime = millis();
    }
}

/**
 * @brief Reads WiFi/Network settings from NVS (Flash).
 * Defaults are applied if no settings are found.
 */
void AppNetworkManager::loadConfig() {
    _prefs.begin(PREF_NS, true);
    _config.ssid = _prefs.getString("w_ssid", "");
    _config.pass = _prefs.getString("w_pass", "");
    _config.useStatic = _prefs.getBool("w_static", DEFAULT_WIFI_STATIC);
    _config.ip.fromString(_prefs.getString("w_ip", DEFAULT_WIFI_IP));
    _config.gateway.fromString(_prefs.getString("w_gw", DEFAULT_WIFI_GATEWAY));
    _config.subnet.fromString(_prefs.getString("w_sub", DEFAULT_WIFI_SUBNET));
    _config.ledStatusMode = _prefs.getBool("w_led", true);
    _prefs.end();
}

void AppNetworkManager::saveConfig(const WifiConfig& cfg) {
    _prefs.begin(PREF_NS, false);
    _prefs.putString("w_ssid", cfg.ssid);
    _prefs.putString("w_pass", cfg.pass);
    _prefs.putBool("w_static", cfg.useStatic);
    _prefs.putString("w_ip", cfg.ip.toString());
    _prefs.putString("w_gw", cfg.gateway.toString());
    _prefs.putString("w_sub", cfg.subnet.toString());
    _prefs.putBool("w_led", cfg.ledStatusMode);
    _prefs.end();
    _config = cfg;
}

/**
 * @brief Initiates a WiFi connection attempt.
 * This function is called during begin() and by the reconnection logic.
 */
void AppNetworkManager::connect() {
    reconnect(); // Delegate to reconnect logic
}

/**
 * @brief Internal method to handle WiFi reconnection.
 */
void AppNetworkManager::reconnect() {
    if (_config.ssid.length() == 0) {
        Serial.println("No SSID configured, cannot reconnect.");
        return;
    }
    if (_config.useStatic) {
        WiFi.config(_config.ip, _config.gateway, _config.subnet);
    } else {
        // Ensure DHCP by clearing static config
        WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
    }
    Serial.print("Connecting to WiFi: ");
    Serial.println(_config.ssid);
    WiFi.begin(_config.ssid.c_str(), _config.pass.c_str());
}

/**
 * @brief Handles various WiFi events.
 */
void AppNetworkManager::handleWiFiEvent(WiFiEvent_t event) {
    switch (event) {
        case ARDUINO_EVENT_WIFI_STA_START:
            Serial.println("WiFi STA Started.");
            break;
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            Serial.println("WiFi STA Disconnected.");
            _wifiConnected = false;
            break;
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            Serial.print("WiFi STA Connected. IP: ");
            Serial.println(WiFi.localIP());
            _wifiConnected = true;
            _reconnectAttemptTime = 0; // Reset timer on successful connection
            break;
        case ARDUINO_EVENT_WIFI_AP_START:
            Serial.println("WiFi AP Started.");
            break;
        default: break;
    }
}

/**
 * @brief Monitors BOOT button for Factory Reset.
 * Triggers if held for 5 seconds. Provides visual feedback with Red LED blinking.
 */
void handleFactoryReset() {
    static unsigned long start = 0;
    if (digitalRead(BOOT_BUTTON_PIN) == LOW) {
        if (start == 0) start = millis();
        if (millis() - start > FACTORY_RESET_TIME) {
            // Visual feedback: Fast Red Blink
            for(int i=0; i<10; i++) {
                Hardware.setLedColor(255, 0, 0, 255); delay(100);
                Hardware.setLedColor(0, 0, 0, 0); delay(100);
            }
            Preferences p; p.begin(PREF_NS, false); p.clear(); p.end();
            ESP.restart();
        }
    } else {
        start = 0;
    }
}

AppNetworkManager AppNet;

/**
 * @brief Returns the current WiFi connection status.
 */
bool AppNetworkManager::isConnected() const {
    return _wifiConnected;
}

/**
 * @brief Sets the LED status mode and saves it to NVS.
 */
void AppNetworkManager::setLedStatusMode(bool mode) {
    _config.ledStatusMode = mode;
    saveConfig(_config);
}