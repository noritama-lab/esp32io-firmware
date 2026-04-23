/**
 * @file HardwareManager.cpp
 * @brief ハードウェア周辺機器管理の実装。
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
 * @brief すべてのピンと周辺機器を設定します。
 * デフォルトのPWM設定をロードし、NeoPixelを準備します。
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

int HardwareManager::readDO(int id) {
    if (id < 0 || id >= DIO_OUT_COUNT) return 0;
    return digitalRead(DIO_OUT_PINS[id]);
}

void HardwareManager::writeDO(int id, int value) {
    if (id < 0 || id >= DIO_OUT_COUNT) return;
    digitalWrite(DIO_OUT_PINS[id], value ? HIGH : LOW);
}

/**
 * @brief マルチサンプリングによるADC読み取り。
 * @param id Config.hで定義されたADCインデックス。
 * @return 平均化されたADC値 (ミリボルト)。
 */
int HardwareManager::readADCValue(int id) {
    if (id < 0 || id >= ADC_COUNT) return 0;
    long sum = 0;
    for (int i = 0; i < ADC_SAMPLES; i++) sum += analogReadMilliVolts(ADC_PINS[id]);
    return static_cast<int>(sum / ADC_SAMPLES);
}

/**
 * @brief PWMの周波数と分解能を再設定します。
 * デューティは新しい分解能に合わせて自動的にスケーリングされます。
 */
bool HardwareManager::applyPwmConfig(int freq, int res) {
    bool success = true;
    long oldMax = calculateMaxDuty(_pwmSettings.res);
    long newMax = calculateMaxDuty(res);

    _pwmSettings.freq = freq;
    _pwmSettings.res = res;

    for (int i = 0; i < PWM_COUNT; i++) {
        ledcDetach(PWM_PINS[i]);
        if (!ledcAttach(PWM_PINS[i], freq, res)) success = false;
        
        _pwmSettings.duties[i] = (_pwmSettings.duties[i] * newMax) / oldMax;
        ledcWrite(PWM_PINS[i], _pwmSettings.duties[i]);
    }
    return success;
}

/**
 * @brief 個々のPWMチャネルのデューティサイクルを更新します。
 * @param id PWMインデックス。
 * @param duty 新しいデューティ値（安全な最大値に制限されます）。
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
 * @brief 特定の分解能でのオーバーフローを防ぐため、安全な最大デューティを返します。
 */
int HardwareManager::getSafeMaxDuty() const {
    int maxD = calculateMaxDuty(_pwmSettings.res);
    return (_pwmSettings.res >= 14) ? maxD - 1 : maxD;
}

/**
 * @brief 内蔵RGB LEDの色と明るさを設定します。
 * APIステータス報告用に、最後に設定された値を保存します。
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
 * @brief 内蔵NeoPixelを使用してシステムステータスを可視化します。
 * - 接続済み: 呼吸する緑色のエフェクト (約4秒周期)。
 * - 切断: 青色の点滅 (500ms)。
 * - 無効: 黄色の点灯。
 */
void HardwareManager::updateStatusLed(bool wifiEnabled, bool wifiConnected) {
    static unsigned long lastUpdate = 0;
    static bool state = false;

    if (!wifiEnabled) {
        setLedColor(255, 255, 0, 5); // Yellow solid (dimmed)
        return;
    }

    if (wifiConnected) {
        if (millis() - lastUpdate < 33) return; // 接続中は30fps
        lastUpdate = millis();
        // 呼吸エフェクト: 輝度を0-10にマッピングするサイン波 (控えめな緑)
        float duty = (sin(millis() * 0.0015f) + 1.0f) / 2.0f;
        uint8_t br = (uint8_t)(duty * 10);
        setLedColor(0, 255, 0, br);
    } else {
        // 未接続・接続試行中は割り込み禁止時間を減らすため、更新頻度を劇的に下げる(100ms)
        if (millis() - lastUpdate < 100) return;
        lastUpdate = millis();

        // 点滅パターン: 500msごとに青色を切り替え
        static unsigned long lastToggle = 0;
        if (millis() - lastToggle > 500) {
            state = !state;
            state ? setLedColor(0, 0, 255, 10) : setLedColor(0, 0, 0);
            lastToggle = millis();
        }
    }
}

HardwareManager Hardware;