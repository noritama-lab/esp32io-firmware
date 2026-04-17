/**
 * @file HardwareManager.h
 * @brief Peripheral control (GPIO, ADC, PWM, NeoPixel) for ESP32-S3.
 * @copyright Copyright (c) 2024 norit. Licensed under the MIT License.
 */
#ifndef HARDWARE_MANAGER_H
#define HARDWARE_MANAGER_H

#include "Config.h"
#include <Adafruit_NeoPixel.h>

/**
 * @class HardwareManager
 * @brief Abstracts hardware interactions.
 */
class HardwareManager {
public:
    HardwareManager();
    void begin(); // Initializes pins and peripherals
    
    // --- DIO ---
    int readDI(int id);
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

    // --- Built-in RGB LED ---
    void setLedColor(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness = 0);
    /** @brief Updates LED with breathing effect or blink pattern based on connection status. */
    void updateStatusLed(bool wifiConnected);
    void getLedColor(uint8_t &r, uint8_t &g, uint8_t &b, uint8_t &br) const;

private:
    Adafruit_NeoPixel _statusLed;
    PwmSettings _pwmSettings;
    int calculateMaxDuty(int res) const;
    uint8_t _lastR = 0, _lastG = 0, _lastB = 0, _lastBr = 0;
};

extern HardwareManager Hardware;
#endif