/**
 * @file HardwareManager.cpp
 * @brief Implementation of hardware peripheral management.
 * @copyright Copyright (c) 2024 norit. Licensed under the MIT License.
 */
#include "HardwareManager.h"

HardwareManager::HardwareManager() 
    : _statusLed(1, RGB_PIN, NEO_GRB + NEO_KHZ800) {
    _pwmSettings.freq = DEFAULT_PWM_FREQ;
    _pwmSettings.res = DEFAULT_PWM_RES;
    for(int i=0; i<PWM_COUNT; i++) _pwmSettings.duties[i] = 0;
}

/**
 * @brief Set up all pins and peripherals.
 * Loads default PWM settings and prepares NeoPixel.
 */
void HardwareManager::begin() {
    for (int i = 0; i < DIO_IN_COUNT; i++) pinMode(DIO_IN_PINS[i], INPUT);
    for (int i = 0; i < DIO_OUT_COUNT; i++) {
        pinMode(DIO_OUT_PINS[i], OUTPUT);
        digitalWrite(DIO_OUT_PINS[i], LOW);
    }
    applyPwmConfig(_pwmSettings.freq, _pwmSettings.res);
    _statusLed.begin();
    setLedColor(0, 0, 0, DEFAULT_BRIGHTNESS);
}

int HardwareManager::readDI(int id) {
    if (id < 0 || id >= DIO_IN_COUNT) return 0;
    return digitalRead(DIO_IN_PINS[id]);
}

void HardwareManager::writeDO(int id, int value) {
    if (id < 0 || id >= DIO_OUT_COUNT) return;
    digitalWrite(DIO_OUT_PINS[id], value ? HIGH : LOW);
}

/**
 * @brief Reads ADC with multi-sampling.
 * @param id ADC index defined in Config.h.
 * @return Averaged 12-bit ADC value.
 */
int HardwareManager::readADCValue(int id) {
    if (id < 0 || id >= ADC_COUNT) return 0;
    long sum = 0;
    for (int i = 0; i < ADC_SAMPLES; i++) sum += analogRead(ADC_PINS[id]);
    return static_cast<int>(sum / ADC_SAMPLES);
}

/**
 * @brief Re-configures PWM frequency and resolution.
 * Duties are scaled automatically to match new resolution.
 */
bool HardwareManager::applyPwmConfig(int freq, int res) {
    int oldMax = calculateMaxDuty(_pwmSettings.res);
    int newMax = calculateMaxDuty(res);

    _pwmSettings.freq = freq;
    _pwmSettings.res = res;

    bool success = true;
    for (int i = 0; i < PWM_COUNT; i++) {
        ledcDetach(PWM_PINS[i]);
        if (!ledcAttach(PWM_PINS[i], freq, res)) success = false;
        
        // 解像度に合わせてデューティをスケーリング
        _pwmSettings.duties[i] = (static_cast<long>(_pwmSettings.duties[i]) * newMax) / oldMax;
        ledcWrite(PWM_PINS[i], _pwmSettings.duties[i]);
    }
    return success;
}

/**
 * @brief Updates individual PWM channel duty cycle.
 * @param id PWM index.
 * @param duty New duty value (constrained to safe max).
 */
void HardwareManager::setPwmDuty(int id, int duty) {
    if (id < 0 || id >= PWM_COUNT) return;
    int maxD = getSafeMaxDuty();
    _pwmSettings.duties[id] = constrain(duty, 0, maxD);
    ledcWrite(PWM_PINS[id], _pwmSettings.duties[id]);
}

int HardwareManager::calculateMaxDuty(int res) const {
    return (1UL << res) - 1;
}

/**
 * @brief Returns safe max duty to prevent overflow on certain resolutions.
 */
int HardwareManager::getSafeMaxDuty() const {
    int maxD = calculateMaxDuty(_pwmSettings.res);
    return (_pwmSettings.res >= 14) ? maxD - 1 : maxD;
}

/**
 * @brief Set built-in RGB LED color and brightness.
 * Stores last set values for API status reporting.
 */
void HardwareManager::setLedColor(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness) {
    if (brightness > 0) _statusLed.setBrightness(brightness);
    _statusLed.setPixelColor(0, _statusLed.Color(r, g, b));
    _statusLed.show();
    _lastR = r; _lastG = g; _lastB = b;
    if (brightness > 0) _lastBr = brightness;
}

void HardwareManager::getLedColor(uint8_t &r, uint8_t &g, uint8_t &b, uint8_t &br) const {
    r = _lastR; g = _lastG; b = _lastB; br = _lastBr;
}

/**
 * @brief Visualizes system status using built-in NeoPixel.
 * - Connected: Breathing Green effect (~4sec period).
 * - Disconnected: Blinking Blue (500ms).
 */
void HardwareManager::updateStatusLed(bool wifiConnected) {
    static unsigned long lastUpdate = 0;
    static bool state = false;

    // Limit update frequency to ~30fps to reduce CPU/show() overhead
    if (millis() - lastUpdate < 33) return;
    lastUpdate = millis();

    if (wifiConnected) {
        // Breathing effect: Sine wave mapping brightness 0-10 (Dimmer green)
        float duty = (sin(millis() * 0.0015f) + 1.0f) / 2.0f;
        uint8_t br = (uint8_t)(duty * 10);
        setLedColor(0, 255, 0, br);
    } else {
        // Blink pattern: Blue toggle every 500ms
        static unsigned long lastToggle = 0;
        if (millis() - lastToggle > 500) {
            state = !state;
            state ? setLedColor(0, 0, 255, 20) : setLedColor(0, 0, 0);
            lastToggle = millis();
        }
    }
}

HardwareManager Hardware;