/**
 * @file HardwareManager.h
 * @brief ESP32-S3の周辺機器制御 (GPIO, ADC, PWM, NeoPixel)。
 * 
 * このファイルは、ESP32-S3ボード上の様々なハードウェア周辺機器（GPIO、ADC、PWM、
 * NeoPixel LED、I2Cセンサー、OLEDディスプレイなど）の初期化と制御を提供します。
 * 抽象化されたインターフェースを通じて、これらのハードウェア機能にアクセスできます。
 * @copyright Copyright (c) 2024 norit. Licensed under the MIT License.
 */
#ifndef HARDWARE_MANAGER_H
#define HARDWARE_MANAGER_H

#include "Config.h"
#include <Adafruit_NeoPixel.h>
#include <Adafruit_BME280.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_VL53L1X.h>
#include <Wire.h>

/**
 * @class HardwareManager
 * @brief ハードウェアとのやり取りを抽象化します。
 * 
 * このクラスは、ESP32-S3の物理ピンと接続された周辺機器を管理するための
 * 単一インターフェースを提供します。
 */
class HardwareManager {
public:
    /**
     * @brief HardwareManagerクラスのコンストラクタ。
     * NeoPixel LED、OLEDディスプレイ、PWM設定の初期化を行います。
     */
    HardwareManager();
    /**
     * @brief すべてのハードウェアピンと周辺機器を初期化します。
     * デジタル入力/出力ピンの設定、PWMの初期化、I2Cバスの開始、
     * および接続されているI2Cセンサーの検出と初期化を行います。
     */
    void begin();
    
    // --- DIO ---
    /**
     * @brief 指定されたデジタル入力ピンの状態を読み取ります。
     * @param id 読み取るデジタル入力ピンのインデックス (0からDIO_IN_COUNT-1)。
     * @return ピンの状態 (HIGH=1, LOW=0)。無効なIDの場合は0を返します。
     */
    int readDI(int id);
    /**
     * @brief 指定されたデジタル出力ピンの論理状態を読み取ります。
     * DIO出力反転設定を考慮して、ユーザーが設定した論理値（1または0）を返します。
     * @param id 読み取るデジタル出力ピンのインデックス (0からDIO_OUT_COUNT-1)。
     * @return ピンの論理状態 (1=ON, 0=OFF)。無効なIDの場合は0を返します。
     */
    int readDO(int id);
    /**
     * @brief 指定されたデジタル出力ピンに値を書き込みます。
     * DIO出力反転設定に基づいて、物理ピンの状態 (HIGH/LOW) を決定します。
     * @param id 書き込むデジタル出力ピンのインデックス (0からDIO_OUT_COUNT-1)。
     * @param value 設定する論理値 (1=ON, 0=OFF)。
     */
    void writeDO(int id, int value);
    
    // --- ADC ---
    /**
     * @brief マルチサンプリングによるアナログ-デジタル変換 (ADC) 値を読み取ります。
     * @param id 読み取るADCピンのインデックス (0からADC_COUNT-1)。
     * @return 複数回のサンプリングで平均化されたADC値 (生の値)。無効なIDの場合は0を返します。
     */
    int readADCValue(int id);
    
    // --- PWM ---
    /**
     * @brief PWM (パルス幅変調) の周波数と分解能を再設定します。
     * 既存のデューティサイクルは、新しい分解能に合わせて自動的にスケーリングされます。
     * @param freq 設定する周波数 (Hz)。
     * @param res 設定する分解能 (ビット数)。
     * @return 設定が成功した場合はtrue、失敗した場合はfalseを返します。
     */
    bool applyPwmConfig(int freq, int res);
    /**
     * @brief 個々のPWMチャネルのデューティサイクルを更新します。
     * デューティ値は、現在のPWM分解能に基づいて安全な最大値に制限されます。
     * @param id 設定するPWMチャネルのインデックス (0からPWM_COUNT-1)。
     * @param duty 設定するデューティ値。
     */
    void setPwmDuty(int id, int duty);
    /**
     * @brief 現在のPWM分解能における安全な最大デューティ値を返します。
     * @return 安全な最大デューティ値。
     */
    int getSafeMaxDuty() const;
    /** @brief 指定されたPWMチャネルの現在のデューティ値を取得します。 */
    int getDuty(int id) const { return _pwmSettings.duties[id]; }
    /** @brief 現在のPWM周波数設定を取得します。 */
    int getFreq() const { return _pwmSettings.freq; }
    /** @brief 現在のPWM分解能設定を取得します。 */
    int getRes() const { return _pwmSettings.res; }

    // --- 内蔵 RGB LED ---
    /**
     * @brief 内蔵RGB LED (NeoPixel) の色と明るさを設定します。
     * @param r 赤色の成分 (0-255)。
     * @param g 緑色の成分 (0-255)。
     * @param b 青色の成分 (0-255)。
     * @param brightness LEDの明るさ (0-255)。0の場合、以前の明るさが維持されます。
     */
    void setLedColor(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness = 0);
    /**
     * @brief 内蔵NeoPixel LEDを使用してシステムステータスを視覚的に表示します。
     * WiFiの有効/無効、接続状態に応じて異なるパターンでLEDを点灯させます。
     * @param wifiEnabled WiFi機能が有効かどうか。
     * @param wifiConnected WiFiがアクセスポイントに接続されているかどうか。
     */
    void updateStatusLed(bool wifiEnabled, bool wifiConnected);
    /**
     * @brief 現在設定されているRGB LEDの色と明るさの値を取得します。
     * @param r 赤色の成分を格納する参照変数。
     * @param g 緑色の成分を格納する参照変数。
     * @param b 青色の成分を格納する参照変数。
     * @param br 明るさの成分を格納する参照変数。
     */
    void getLedColor(uint8_t &r, uint8_t &g, uint8_t &b, uint8_t &br) const;

    // --- I2C ---
    /**
     * @brief I2Cバス上に指定したアドレスのデバイスが存在するか確認します。
     * @param address 確認するI2Cアドレス。
     * @return デバイスが応答した場合はtrue。
     */
    bool probeDevice(uint8_t address);
    /**
     * @brief I2Cバス上のデバイスをスキャンし、見つかったデバイスのアドレスをリストアップします。
     * @param foundDevices 見つかったデバイスのアドレスを格納する配列。
     * @param count 見つかったデバイスの数を格納する参照変数。
     * @param maxCount foundDevices配列の最大容量。
     */
    void scanI2C(uint8_t* foundDevices, int& count, int maxCount);
    /**
     * @brief 指定されたI2Cアドレスにデータを書き込みます。
     * @param addr 書き込み対象のI2Cデバイスアドレス。
     * @param data 書き込むデータのバイト配列へのポインタ。
     * @param len 書き込むデータの長さ。
     * @return `Wire.endTransmission()`の戻り値 (0: 成功, 1: データが長すぎる, 2: NACK on transmit of address, 3: NACK on transmit of data, 4: その他のエラー)。
     */
    uint8_t writeI2C(uint8_t addr, const uint8_t* data, size_t len);
    /**
     * @brief 指定されたI2Cアドレスからデータを読み取ります。
     * @param addr 読み取り対象のI2Cデバイスアドレス。
     * @param buffer 読み取ったデータを格納するバイト配列へのポインタ。
     * @param len 読み取るデータの長さ。
     * @return 実際に読み取られたバイト数。
     */
    uint8_t readI2C(uint8_t addr, uint8_t* buffer, size_t len);

    // --- 特定デバイス対応 ---
    /**
     * @brief BME280センサーから温度、湿度、気圧データを読み取ります。
     * @param t 温度を格納するfloat参照変数。
     * @param h 湿度を格納するfloat参照変数。
     * @param p 気圧を格納するfloat参照変数。
     * @return BME280が初期化されており、データが読み取れた場合はtrue、それ以外はfalse。
     */
    bool getBME280Data(float &t, float &h, float &p);
    /**
     * @brief MPU6050センサーから加速度計とジャイロスコープのデータを読み取ります。
     * @param accel 加速度データを格納する3要素のfloat配列 (x, y, z)。
     * @param gyro ジャイロスコープデータを格納する3要素のfloat配列 (x, y, z)。
     * @return MPU6050が初期化されており、データが読み取れた場合はtrue、それ以外はfalse。
     */
    bool getMPU6050Data(float accel[3], float gyro[3]);
    /**
     * @brief VL53L1X距離センサーから距離データを読み取ります。
     * @param mm 読み取られた距離 (ミリメートル) を格納するuint16_t参照変数。
     * @return VL53L1Xが初期化されており、有効な距離データが読み取れた場合はtrue、それ以外はfalse。
     */
    bool getVL53L1XDistance(uint16_t &mm);
    /**
     * @brief OLEDディスプレイにテキストを表示します。
     * @param text 表示する文字列。
     * @param x テキストの開始X座標。
     * @param y テキストの開始Y座標。
     * @param size テキストのフォントサイズ (1倍、2倍など)。
     * @param clear trueの場合、表示前にディスプレイをクリアします。
     */
    void updateOLED(const char* text, int x = 0, int y = 0, int size = 1, bool clear = true);

    // --- デバイス状態取得 ---
    /** @brief 各デバイスが正常に初期化され、通信可能かどうかを返します。 */
    bool isBmeReady() const { return _bmeInit; }
    bool isMpuReady() const { return _mpuInit; }
    bool isVl53Ready() const { return _vl53Init; }
    bool isOledReady() const { return _oledInit; }

private:
    Adafruit_NeoPixel _statusLed;
    PwmSettings _pwmSettings;
    I2CSettings _i2cSettings; // I2C設定を保持
    /**
     * @brief 指定された分解能におけるPWMの最大デューティ値を計算します。
     * @param res PWMの分解能 (ビット数)。
     * @return 計算された最大デューティ値。
     */
    int calculateMaxDuty(int res) const; 
    uint8_t _lastR = 0, _lastG = 0, _lastB = 0, _lastBr = 0; /**< 最後に設定されたRGB LEDの色と明るさ。 */

    Adafruit_BME280 _bme;
    Adafruit_MPU6050 _mpu;
    Adafruit_SSD1306 _display;
    Adafruit_VL53L1X _vl53;
    bool _bmeInit = false, _mpuInit = false, _oledInit = false, _vl53Init = false;
};

extern HardwareManager Hardware;
#endif