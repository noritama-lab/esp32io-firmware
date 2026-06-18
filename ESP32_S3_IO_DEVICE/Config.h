/**
 * @file Config.h
 * @brief ESP32-S3 リモートIOのシステム設定とピン定義。
 * 
 * このファイルには、ESP32-S3リモートIOデバイスのハードウェアピンマッピング、
 * ネットワーク設定のデフォルト値、およびその他のシステム定数が含まれています。
 * これらの定義は、デバイスの動作をカスタマイズするために使用されます。
 * @copyright Copyright (c) 2024 norit. Licensed under the MIT License.
 */
#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// --- デバイス情報 ---
#define DEVICE_NAME_BASE "ESP32_S3_IO" // mDNS名およびAP SSIDのプレフィックスとして使用

// --- ピンマッピング (ESP32-S3 DevKitC-1) ---
const int DIO_IN_PINS[]   = {4, 5, 6, 7, 8, 9};
const int DIO_OUT_PINS[]  = {10, 11, 12, 13, 14, 15};
const int ADC_PINS[]      = {1, 2};
const int PWM_PINS[]      = {36, 37};
const int BOOT_BUTTON_PIN = 0;   // 内蔵Bootボタン (ファクトリーリセット用)
const int RGB_PIN         = 38;  // 内蔵WS2812 (NeoPixel)

// --- I2C 設定 ---
const int DEFAULT_I2C_SDA          = 40;
const int DEFAULT_I2C_SCL          = 41;
const uint32_t DEFAULT_I2C_FREQ    = 100000; // 100kHz

// --- I2C デバイスアドレス ---
const uint8_t ADDR_BME280_A        = 0x76;
const uint8_t ADDR_BME280_B        = 0x77;
const uint8_t ADDR_MPU6050         = 0x68;
const uint8_t ADDR_VL53L1X         = 0x29;
const uint8_t ADDR_OLED            = 0x3C;

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
const unsigned long WIFI_RECONNECT_INTERVAL_MS = 2000; // WiFi再接続を試みる間隔 (ミリ秒)

// --- タイミング定数 ---
const unsigned long FACTORY_RESET_TIME = 5000; // 5秒間長押しでリセット

// --- デフォルトのペリフェラル設定 ---
const int DEFAULT_PWM_FREQ = 5000;
const int DEFAULT_PWM_RES  = 8;
const uint8_t DEFAULT_BRIGHTNESS = 64;
const bool DEFAULT_DIO_OUT_INVERTED = false; // デフォルトは 1=HIGH, 0=LOW
const int ADC_SAMPLES = 8;

/**
 * @brief WiFiおよびネットワーク設定を保持する構造体。
 * NVS (不揮発性ストレージ) に保存され、デバイスの再起動後も設定が維持されます。
 */
struct WifiConfig {
    String ssid; /**< 接続するWiFiアクセスポイントのSSID。 */
    String pass; /**< 接続するWiFiのパスワード。 */
    bool useStatic;
    IPAddress ip;
    IPAddress gateway;
    IPAddress subnet;
    bool ledStatusMode; // true: システムステータス表示, false: マニュアルAPI制御
    bool wifiEnabled;   // true: WiFi有効, false: シリアル専用 (WiFi STAオフ)
    bool dioOutInverted; // true: 論理1で物理LOW, false: 論理1で物理HIGH
};

/**
 * @brief PWMの周波数、分解能、現在のデューティを保持する構造体。
 * PWMチャネルごとの設定を管理します。
 */
struct PwmSettings {
    int freq;
    int res;
    int duties[PWM_COUNT]; /**< 各PWMチャネルの現在のデューティ値。 */
};

/**
 * @brief I2Cバスの設定を保持する構造体。
 * SDAピン、SCLピン、およびバス周波数を管理します。
 */
struct I2CSettings {
    int sda; /**< I2Cデータピン。 */
    int scl; /**< I2Cクロックピン。 */
    uint32_t freq; /**< I2Cバス周波数 (Hz)。 */
};

#endif