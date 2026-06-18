/**
 * @file HardwareManager.cpp
 * @brief ハードウェア周辺機器管理の実装。
 * 
 * このファイルは、ESP32-S3ボード上の様々なハードウェア周辺機器（GPIO、ADC、PWM、
 * NeoPixel LED、I2Cセンサー、OLEDディスプレイなど）の初期化と制御を提供します。
 * 抽象化されたインターフェースを通じて、これらのハードウェア機能にアクセスできます。
 * @copyright Copyright (c) 2024 norit. Licensed under the MIT License.
 */
#include "HardwareManager.h"
#include "NetworkManager.h"

HardwareManager::HardwareManager() 
    : _statusLed(1, RGB_PIN, NEO_GRB + NEO_KHZ800),
      _display(128, 64, &Wire, -1) // OLEDディスプレイの初期化 (リセットピンは-1で未接続)
{
    // PWM設定のデフォルト値を初期化
    _pwmSettings.freq = DEFAULT_PWM_FREQ;
    _pwmSettings.res = DEFAULT_PWM_RES;
    // I2C設定のデフォルト値を初期化
    _i2cSettings.sda = DEFAULT_I2C_SDA;
    _i2cSettings.scl = DEFAULT_I2C_SCL;
    _i2cSettings.freq = DEFAULT_I2C_FREQ;
    for(int i=0; i<PWM_COUNT; i++) _pwmSettings.duties[i] = 0;
}

/**
 * @brief すべてのハードウェアピンと周辺機器を初期化します。
 * デジタル入力/出力ピンの設定、PWMの初期化、I2Cバスの開始、
 * および接続されているI2Cセンサー（BME280, MPU6050, VL53L0X, OLED）の検出と初期化を行います。
 * また、内蔵NeoPixel LEDも初期化されます。
 */
void HardwareManager::begin() {
    // デジタル入力ピンを設定
    for (int i = 0; i < DIO_IN_COUNT; i++) pinMode(DIO_IN_PINS[i], INPUT);
    // デジタル出力ピンを設定し、初期状態を論理0 (OFF) に設定
    for (int i = 0; i < DIO_OUT_COUNT; i++) {
        pinMode(DIO_OUT_PINS[i], OUTPUT);
        writeDO(i, 0); // ロード済みの設定に基づき、論理0(OFF)を物理ピンに反映
    }
    applyPwmConfig(_pwmSettings.freq, _pwmSettings.res);
    Wire.begin(_i2cSettings.sda, _i2cSettings.scl, _i2cSettings.freq);
    delay(200); // I2Cバスの安定化とデバイスの起動待ちを少し延長

    // --- I2Cデバイスの初期化 (追加時はここを編集) ---
    
    // BME280: 温度・湿度・気圧
    if (probeDevice(ADDR_BME280_A) || probeDevice(ADDR_BME280_B)) {
        _bmeInit = _bme.begin(probeDevice(ADDR_BME280_A) ? ADDR_BME280_A : ADDR_BME280_B, &Wire);
    }

    // MPU6050: 加速度・ジャイロ
    if (probeDevice(ADDR_MPU6050)) {
        _mpuInit = _mpu.begin(ADDR_MPU6050, &Wire);
    }

    // VL53L1X: 距離
    if (probeDevice(ADDR_VL53L1X)) {
        // 初期化前にバスを一度落ち着かせる
        Wire.setClock(100000); 
        delay(200);
        for (int retry = 0; retry < 3; retry++) {
            if (_vl53.begin(ADDR_VL53L1X, &Wire)) {
                _vl53Init = true;
                _vl53.setTimingBudget(50); // 50ms
                _vl53.startRanging();
                break;
            }
            if (retry < 2) {
                delay(200); // 失敗時は少し長めに待機
            } else {
            }
        }
    }

    // SSD1306: OLEDディスプレイ
    if (probeDevice(ADDR_OLED)) {
        _oledInit = _display.begin(SSD1306_SWITCHCAPVCC, ADDR_OLED);
        delay(50);
        _display.clearDisplay();
        _display.display();
    }

    // NeoPixel LEDの初期化と初期色の設定
    _statusLed.begin();
    setLedColor(0, 0, 0, DEFAULT_BRIGHTNESS);
}

/**
 * @brief 指定されたデジタル入力ピンの状態を読み取ります。
 * @param id 読み取るデジタル入力ピンのインデックス (0からDIO_IN_COUNT-1)。
 * @return ピンの状態 (HIGH=1, LOW=0)。無効なIDの場合は0を返します。
 */
int HardwareManager::readDI(int id) {
    if (id < 0 || id >= DIO_IN_COUNT) return 0;
    return digitalRead(DIO_IN_PINS[id]);
}

/**
 * @brief 指定されたデジタル出力ピンの論理状態を読み取ります。
 * DIO出力反転設定を考慮して、ユーザーが設定した論理値（1または0）を返します。
 * @param id 読み取るデジタル出力ピンのインデックス (0からDIO_OUT_COUNT-1)。
 * @return ピンの論理状態 (1=ON, 0=OFF)。無効なIDの場合は0を返します。
 */
int HardwareManager::readDO(int id) {
    if (id < 0 || id >= DIO_OUT_COUNT) return 0;
    bool inv = AppNet.getConfig().dioOutInverted;
    int physVal = digitalRead(DIO_OUT_PINS[id]);
    if (inv) return (physVal == LOW) ? 1 : 0;
    return (physVal == HIGH) ? 1 : 0;
}

/**
 * @brief 指定されたデジタル出力ピンに値を書き込みます。
 * DIO出力反転設定に基づいて、物理ピンの状態 (HIGH/LOW) を決定します。
 * @param id 書き込むデジタル出力ピンのインデックス (0からDIO_OUT_COUNT-1)。
 * @param value 設定する論理値 (1=ON, 0=OFF)。
 */
void HardwareManager::writeDO(int id, int value) {
    if (id < 0 || id >= DIO_OUT_COUNT) return;
    bool inv = AppNet.getConfig().dioOutInverted;
    if (inv) {
        digitalWrite(DIO_OUT_PINS[id], value ? LOW : HIGH); // 反転: 1=LOW, 0=HIGH
    } else {
        digitalWrite(DIO_OUT_PINS[id], value ? HIGH : LOW); // 正論理: 1=HIGH, 0=LOW
    }
}

/**
 * @brief マルチサンプリングによるアナログ-デジタル変換 (ADC) 値を読み取ります。
 * @param id 読み取るADCピンのインデックス (0からADC_COUNT-1)。
 * @return 複数回のサンプリングで平均化されたADC値 (生の値)。無効なIDの場合は0を返します。
 */
int HardwareManager::readADCValue(int id) {
    if (id < 0 || id >= ADC_COUNT) return 0;
    long sum = 0;
    for (int i = 0; i < ADC_SAMPLES; i++) sum += analogRead(ADC_PINS[id]);
    return static_cast<int>(sum / ADC_SAMPLES);
}

/**
 * @brief PWM (パルス幅変調) の周波数と分解能を再設定します。
 * 既存のデューティサイクルは、新しい分解能に合わせて自動的にスケーリングされます。
 * @param freq 設定する周波数 (Hz)。
 * @param res 設定する分解能 (ビット数)。
 * @return 設定が成功した場合はtrue、失敗した場合はfalseを返します。
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
 * デューティ値は、現在のPWM分解能に基づいて安全な最大値に制限されます。
 * @param id 設定するPWMチャネルのインデックス (0からPWM_COUNT-1)。
 * @param duty 設定するデューティ値。
 */
void HardwareManager::setPwmDuty(int id, int duty) {
    if (id < 0 || id >= PWM_COUNT) return;
    int maxD = getSafeMaxDuty();
    _pwmSettings.duties[id] = constrain(duty, 0, maxD);
    ledcWrite(PWM_PINS[id], _pwmSettings.duties[id]);
}
/**
 * @brief 指定された分解能におけるPWMの最大デューティ値を計算します。
 * @param res PWMの分解能 (ビット数)。
 * @return 計算された最大デューティ値。
 */
int HardwareManager::calculateMaxDuty(int res) const {
    return (1UL << res) - 1;
}

/**
 * @brief 現在のPWM分解能における安全な最大デューティ値を返します。
 * 特に14ビット以上の分解能では、`ledcWrite`の内部処理でオーバーフローを防ぐために
 * 最大値を1減らした値を使用します。
 * @return 安全な最大デューティ値。
 */
int HardwareManager::getSafeMaxDuty() const {
    int maxD = calculateMaxDuty(_pwmSettings.res);
    return (_pwmSettings.res >= 14) ? maxD - 1 : maxD;
}

/**
 * @brief 内蔵RGB LED (NeoPixel) の色と明るさを設定します。
 * 設定された色は、API経由でのステータス報告のために内部に保存されます。
 * @param r 赤色の成分 (0-255)。
 * @param g 緑色の成分 (0-255)。
 * @param b 青色の成分 (0-255)。
 * @param brightness LEDの明るさ (0-255)。0の場合、以前の明るさが維持されます。
 */
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

/**
 * @brief 現在設定されているRGB LEDの色と明るさの値を取得します。
 * @param r 赤色の成分を格納する参照変数。
 * @param g 緑色の成分を格納する参照変数。
 * @param b 青色の成分を格納する参照変数。
 * @param br 明るさの成分を格納する参照変数。
 */
void HardwareManager::getLedColor(uint8_t &r, uint8_t &g, uint8_t &b, uint8_t &br) const {
    r = _lastR; g = _lastG; b = _lastB; br = _lastBr;
}

/**
 * @brief 内蔵NeoPixel LEDを使用してシステムステータスを視覚的に表示します。
 * WiFiの有効/無効、接続状態に応じて異なるパターンでLEDを点灯させます。
 * - WiFi無効: 黄色で点灯 (控えめな明るさ)。
 * - WiFi接続済み: 緑色で呼吸エフェクト (約4秒周期)。
 * - WiFi未接続/接続試行中: 青色で点滅 (500ms周期)。
 * @param wifiEnabled WiFi機能が有効かどうか。
 * @param wifiConnected WiFiがアクセスポイントに接続されているかどうか。
 */
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
        setLedColor(255, 255, 0, 5); // WiFi無効時は黄色で点灯 (控えめ)
        return;
    }

    if (wifiConnected) {
        if (millis() - lastUpdate < 33) return; // 接続中は約30fpsで更新
        lastUpdate = millis();
        // 呼吸エフェクト: 輝度を0-10にマッピングするサイン波 (控えめな緑)
        float duty = (sin(millis() * 0.0015f) + 1.0f) / 2.0f;
        uint8_t br = (uint8_t)(duty * 10);
        setLedColor(0, 255, 0, br);
    } else {
        // 未接続・接続試行中は割り込み禁止時間を減らすため、更新頻度を劇的に下げる(100ms)
        if (millis() - lastUpdate < 100) return; // 未接続時は更新頻度を落とす
        lastUpdate = millis(); // 更新時間を記録

        // 点滅パターン: 500msごとに青色を切り替え
        static unsigned long lastToggle = 0;
        if (millis() - lastToggle > 500) {
            state = !state;
            state ? setLedColor(0, 0, 255, 10) : setLedColor(0, 0, 0);
            lastToggle = millis();
        }
    }
}

bool HardwareManager::probeDevice(uint8_t address) {
    Wire.beginTransmission(address);
    return (Wire.endTransmission() == 0);
}

/**
 * @brief I2Cバス上のデバイスをスキャンし、見つかったデバイスのアドレスをリストアップします。
 * @param foundDevices 見つかったデバイスのアドレスを格納する配列。
 * @param count 見つかったデバイスの数を格納する参照変数。
 * @param maxCount foundDevices配列の最大容量。
 * @note アドレス0x00と0x7Fは予約されているためスキップされます。
 */
void HardwareManager::scanI2C(uint8_t* foundDevices, int& count, int maxCount) {
    count = 0;
    for (uint8_t address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        if (Wire.endTransmission() == 0 && count < maxCount) {
            foundDevices[count++] = address;
        }
    }
}

/**
 * @brief 指定されたI2Cアドレスにデータを書き込みます。
 * @param addr 書き込み対象のI2Cデバイスアドレス。
 * @param data 書き込むデータのバイト配列へのポインタ。
 * @param len 書き込むデータの長さ。
 * @return `Wire.endTransmission()`の戻り値 (0: 成功, 1: データが長すぎる, 2: NACK on transmit of address, 3: NACK on transmit of data, 4: その他のエラー)。
 */
uint8_t HardwareManager::writeI2C(uint8_t addr, const uint8_t* data, size_t len) {
    Wire.beginTransmission(addr);
    for (size_t i = 0; i < len; i++) Wire.write(data[i]);
    return Wire.endTransmission();
}

/**
 * @brief 指定されたI2Cアドレスからデータを読み込みます。
 * @param addr 読み取り対象のI2Cデバイスアドレス。
 * @param buffer 読み取ったデータを格納するバイト配列へのポインタ。
 * @param len 読み取るデータの長さ。
 * @return 実際に読み取られたバイト数。
 */
uint8_t HardwareManager::readI2C(uint8_t addr, uint8_t* buffer, size_t len) {
    uint8_t received = Wire.requestFrom(addr, (uint8_t)len);
    for (uint8_t i = 0; i < received; i++) {
        if (Wire.available()) {
            buffer[i] = Wire.read();
        }
    }
    // 読み取り後にWireバッファに残っている可能性のあるデータをクリア
    while (Wire.available()) {
        Wire.read();
    }
    return received;
}

bool HardwareManager::getBME280Data(float &t, float &h, float &p) {
    if (!_bmeInit) return false;
    t = _bme.readTemperature();
    h = _bme.readHumidity();
    p = _bme.readPressure() / 100.0F;
    return true;
}

/**
 * @brief MPU6050センサーから加速度計とジャイロスコープのデータを読み取ります。
 * @param accel 加速度データを格納する3要素のfloat配列 (x, y, z)。
 * @param gyro ジャイロスコープデータを格納する3要素のfloat配列 (x, y, z)。
 * @return MPU6050が初期化されており、データが読み取れた場合はtrue、それ以外はfalse。
 * @note 読み取られる値は重力加速度 (m/s^2) および角速度 (rad/s) です。
 */
bool HardwareManager::getMPU6050Data(float accel[3], float gyro[3]) {
    if (!_mpuInit) return false;
    sensors_event_t a, g, temp;
    _mpu.getEvent(&a, &g, &temp);
    accel[0] = a.acceleration.x; accel[1] = a.acceleration.y; accel[2] = a.acceleration.z;
    gyro[0] = g.gyro.x; gyro[1] = g.gyro.y; gyro[2] = g.gyro.z;
    return true;
}

/**
 * @brief VL53L1X距離センサーから距離データを読み取ります。
 * @param mm 読み取られた距離 (ミリメートル) を格納するuint16_t参照変数。
 * @return VL53L1Xが初期化されており、有効な距離データが読み取れた場合はtrue、それ以外はfalse。
 */
bool HardwareManager::getVL53L1XDistance(uint16_t &mm) {
    if (!_vl53Init) return false;
    
    if (_vl53.dataReady()) { // データが準備できているか確認
        int16_t distance = _vl53.distance();
        _vl53.clearInterrupt(); // 次の測定のためにインタラプトをクリア

        if (distance >= 0) {
            mm = (uint16_t)distance;
            return true;
        }
    }
    return false; // データが準備できていない、またはエラー時はfalseを返す
}

/**
 * @brief OLEDディスプレイにテキストを表示します。
 * @param text 表示する文字列。
 * @param x テキストの開始X座標。
 * @param y テキストの開始Y座標。
 * @param size テキストのフォントサイズ (1倍、2倍など)。
 * @param clear trueの場合、表示前にディスプレイをクリアします。
 */
void HardwareManager::updateOLED(const char* text, int x, int y, int size, bool clear) {
    if (!_oledInit) return;
    if (clear) _display.clearDisplay();
    
    _display.setCursor(x, y);
    _display.setTextSize(size);
    _display.setTextColor(SSD1306_WHITE);
    _display.println(text);
    _display.display();
}

HardwareManager Hardware;