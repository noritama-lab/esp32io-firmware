/**
 * @file HardwareManager.h
 * @brief ESP32-S3の周辺機器制御 (GPIO, ADC, PWM, NeoPixel)。
 * @copyright Copyright (c) 2024 norit. Licensed under the MIT License.
 */
#ifndef HARDWARE_MANAGER_H
#define HARDWARE_MANAGER_H

#include "Config.h"
#include <Adafruit_NeoPixel.h>

/**
 * @class HardwareManager
 * @brief ハードウェアとのやり取りを抽象化します。
 */
class HardwareManager {
public:
    HardwareManager();
    void begin(); // ピンと周辺機器を初期化します
    
    // --- DIO ---
    int readDI(int id);
    int readDO(int id);
    void writeDO(int id, int value);
    
    // --- ADC ---
    int readADCValue(int id);
    
    // --- PWM ---
    bool applyPwmConfig(int freq, int res);
    void setPwmDuty(int id, int duty);
    int getSafeMaxDuty() const;
    int getDuty(int id) const { return _pwmSettings.duties[id]; }
    int getFreq() const { return _pwmSettings.freq; }
    int getRes() const { return _pwmSettings.res; }

    // --- 内蔵 RGB LED ---
    void setLedColor(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness = 0);
    /** @brief 接続ステータスに基づいて、LEDを呼吸エフェクトまたは点滅パターンで更新します。 */
    void updateStatusLed(bool wifiEnabled, bool wifiConnected);
    void getLedColor(uint8_t &r, uint8_t &g, uint8_t &b, uint8_t &br) const;

private:
    Adafruit_NeoPixel _statusLed;
    PwmSettings _pwmSettings;
    int calculateMaxDuty(int res) const;
    uint8_t _lastR = 0, _lastG = 0, _lastB = 0, _lastBr = 0;
};

extern HardwareManager Hardware;
#endif