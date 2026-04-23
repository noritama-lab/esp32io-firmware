/**
 * @file Config.h
 * @brief System configuration and pin definitions for ESP32-S3 Remote IO.
 * @copyright Copyright (c) 2024 norit. Licensed under the MIT License.
 */
#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// --- Device Info ---
#define DEVICE_NAME_BASE "ESP32_S3_IO" // Used as mDNS name and AP SSID prefix

// --- Pin Mapping (ESP32-S3) ---
const int DIO_IN_PINS[]   = {4, 5, 6, 7, 8, 9};
const int DIO_OUT_PINS[]  = {10, 11, 12, 13, 14, 15};
const int ADC_PINS[]      = {1, 2};
const int PWM_PINS[]      = {38, 39};
const int BOOT_BUTTON_PIN = 0;   // Built-in Boot button for reset
const int RGB_PIN         = 48;  // Built-in WS2812 (NeoPixel)

// --- Helper Macros for Loop Iteration ---
#define DIO_IN_COUNT   (sizeof(DIO_IN_PINS)/sizeof(DIO_IN_PINS[0]))
#define DIO_OUT_COUNT  (sizeof(DIO_OUT_PINS)/sizeof(DIO_OUT_PINS[0]))
#define ADC_COUNT      (sizeof(ADC_PINS)/sizeof(ADC_PINS[0]))
#define PWM_COUNT      (sizeof(PWM_PINS)/sizeof(PWM_PINS[0]))

// --- Networking & Storage Constants ---
const char* const PREF_NS           = "esp32io";
// AP SSID is generated dynamically in NetworkManager: DEVICE_NAME_BASE + MAC_SUFFIX
const char* const DEFAULT_AP_PASS   = "esp32setup";
const bool        DEFAULT_WIFI_STATIC = false;
const char* const DEFAULT_WIFI_IP     = "192.168.1.50";
const char* const DEFAULT_WIFI_GATEWAY= "192.168.1.1";
const char* const DEFAULT_WIFI_SUBNET = "255.255.255.0";
const unsigned long WIFI_RECONNECT_INTERVAL_MS = 10000;

// --- Timing Constants ---
const unsigned long FACTORY_RESET_TIME = 5000; // 5 seconds hold to reset

// --- Default Peripheral Settings ---
const int DEFAULT_PWM_FREQ = 5000;
const int DEFAULT_PWM_RES  = 8;
const uint8_t DEFAULT_BRIGHTNESS = 64;
const int ADC_SAMPLES = 8;

/**
 * @brief Structure to hold WiFi and Network settings.
 */
struct WifiConfig {
    String ssid;
    String pass;
    bool useStatic;
    IPAddress ip;
    IPAddress gateway;
    IPAddress subnet;
    bool ledStatusMode; // true: System status display, false: Manual API control
};

/**
 * @brief Structure to hold PWM frequency, resolution, and current duties.
 */
struct PwmSettings {
    int freq;
    int res;
    int duties[PWM_COUNT];
};

#endif