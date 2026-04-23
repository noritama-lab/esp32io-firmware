/**
 * @file Config.h
 * @brief ESP32-S3 リモートIOのシステム設定とピン定義。
 * @copyright Copyright (c) 2024 norit. Licensed under the MIT License.
 */
#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// --- デバイス情報 ---
#define DEVICE_NAME_BASE "ESP32_S3_IO" // mDNS名およびAP SSIDのプレフィックスとして使用

// --- ピンマッピング (ESP32-S3) ---
const int DIO_IN_PINS[]   = {4, 5, 6, 7, 8, 9};
const int DIO_OUT_PINS[]  = {10, 11, 12, 13, 14, 15};
const int ADC_PINS[]      = {1, 2};
const int PWM_PINS[]      = {38, 39};
const int BOOT_BUTTON_PIN = 0;   // リセット用内蔵Bootボタン
const int RGB_PIN         = 48;  // 内蔵WS2812 (NeoPixel)

// --- ループ反復用ヘルパーマクロ ---
#define DIO_IN_COUNT   (sizeof(DIO_IN_PINS)/sizeof(DIO_IN_PINS[0]))
#define DIO_OUT_COUNT  (sizeof(DIO_OUT_PINS)/sizeof(DIO_OUT_PINS[0]))
#define ADC_COUNT      (sizeof(ADC_PINS)/sizeof(ADC_PINS[0]))
#define PWM_COUNT      (sizeof(PWM_PINS)/sizeof(PWM_PINS[0]))

// --- ネットワーク & ストレージ定数 ---
const char* const PREF_NS           = "esp32io";
// APのSSIDはNetworkManager内で動的に生成されます: DEVICE_NAME_BASE + MAC_SUFFIX
const char* const DEFAULT_AP_PASS   = "esp32setup";
const bool        DEFAULT_WIFI_STATIC = false;
const char* const DEFAULT_WIFI_IP     = "192.168.1.50";
const char* const DEFAULT_WIFI_GATEWAY= "192.168.1.1";
const char* const DEFAULT_WIFI_SUBNET = "255.255.255.0";
const unsigned long WIFI_RECONNECT_INTERVAL_MS = 2000; // 以前の高速な応答に戻す

// --- タイミング定数 ---
const unsigned long FACTORY_RESET_TIME = 5000; // 5秒間長押しでリセット

// --- デフォルトのペリフェラル設定 ---
const int DEFAULT_PWM_FREQ = 5000;
const int DEFAULT_PWM_RES  = 8;
const uint8_t DEFAULT_BRIGHTNESS = 64;
const int ADC_SAMPLES = 8;

/**
 * @brief WiFiおよびネットワーク設定を保持する構造体。
 */
struct WifiConfig {
    String ssid;
    String pass;
    bool useStatic;
    IPAddress ip;
    IPAddress gateway;
    IPAddress subnet;
    bool ledStatusMode; // true: システムステータス表示, false: マニュアルAPI制御
    bool wifiEnabled;   // true: WiFi有効, false: シリアル専用 (WiFi STAオフ)
};

/**
 * @brief PWMの周波数、分解能、現在のデューティを保持する構造体。
 */
struct PwmSettings {
    int freq;
    int res;
    int duties[PWM_COUNT];
};

#endif