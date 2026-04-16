/**
 * @file ESP32_S3_IO_DEVICE_NET.ino
 * @brief ESP32-S3 Remote IO Device Firmware
 * 
 * 主要機能:
 * - デジタル入出力 (DIO) のリモート制御
 * - アナログ入力 (ADC) のサンプリングと取得
 * - 高精度なPWM出力 (周波数・分解能の動的変更)
 * - Webブラウザベースのネットワーク設定インターフェース
 * - JSON形式によるシリアル(USB CDC)およびHTTP API制御
 * - BOOTボタン長押しによるファクトリリセット
 * 
 * @author norit
 * @license MIT
 */

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_NeoPixel.h>
#include "tusb.h"

// ------------------------------------------------------------
// 定数・ピン配列・カウントマクロ
// ------------------------------------------------------------
// --- ピン配列 ---
const int DIO_IN_PINS[]   = {4, 5, 6, 7, 8, 9};
const int DIO_OUT_PINS[]  = {10, 11, 12, 13, 14, 15};
const int ADC_PINS[]      = {1, 2};
const int PWM_PINS[]      = {38, 39};
const int BOOT_BUTTON_PIN = 0;

/** RGB LED (WS2812) 設定 */
const int RGB_PIN = 48; // ESP32-S3 Built-in NeoPixel
const uint8_t DEFAULT_BRIGHTNESS = 64;

/** 各入出力のピン数カウント */
#define DIO_IN_COUNT   (sizeof(DIO_IN_PINS)/sizeof(DIO_IN_PINS[0]))
#define DIO_OUT_COUNT  (sizeof(DIO_OUT_PINS)/sizeof(DIO_OUT_PINS[0]))
#define ADC_COUNT      (sizeof(ADC_PINS)/sizeof(ADC_PINS[0]))
#define PWM_COUNT      (sizeof(PWM_PINS)/sizeof(PWM_PINS[0]))

/** ネットワーク・設定用定数 */
const char* PREF_NS           = "esp32io";
const char* AP_SSID           = "ESP32_S3_IO_SETUP";
const char* AP_PASS           = "esp32setup";
const bool  DEFAULT_WIFI_STATIC = false;
const char* DEFAULT_WIFI_IP     = "192.168.1.50";
const char* DEFAULT_WIFI_GATEWAY= "192.168.1.1";
const char* DEFAULT_WIFI_SUBNET = "255.255.255.0";

/** PWM出力デフォルト設定 */
const int DEFAULT_PWM_FREQ = 5000;
const int DEFAULT_PWM_RES  = 8;

/** ADCサンプリング設定 */
const int ADC_SAMPLES = 8;

// ------------------------------------------------------------
// 実行用グローバル変数
// ------------------------------------------------------------

/** 現在のPWM周波数 */
int PWM_FREQ = DEFAULT_PWM_FREQ;
/** 現在のPWM分解能(bit) */
int PWM_RES  = DEFAULT_PWM_RES;
/** 現在のPWMデューティ保持用配列 */
int PWM_DUTY[PWM_COUNT] = {0};

/** 設定保存用 (Non-volatile storage) */
Preferences prefs;
/** Webサーバ (ポート 80) */
WebServer server(80);

/** 内蔵NeoPixel制御用インスタンス */
Adafruit_NeoPixel statusLed(1, RGB_PIN, NEO_GRB + NEO_KHZ800);

/** 現在のLED状態保持用 */
uint8_t currentR = 0, currentG = 0, currentB = 0;
uint8_t currentBrightness = DEFAULT_BRIGHTNESS;
bool ledOn = false;

// ------------------------------------------------------------
// 構造体定義
// ------------------------------------------------------------
/**
 * WiFi設定（STAモードおよび静的IP）を保持する構造体
 */
struct WifiConfig {
    String ssid;
    String pass;
    bool useStatic;
    IPAddress ip;
    IPAddress gateway;
    IPAddress subnet;
};
WifiConfig wifiCfg;
bool wifiConnected = false;

// ------------------------------------------------------------
// ユーティリティ関数
// ------------------------------------------------------------

/**
 * PWMの分解能に基づいた最大デューティ値を計算します。
 * @return 最大デューティ値 (2^res - 1)
 */
int pwmMaxDuty() {
    return (1UL << PWM_RES) - 1;
}

/**
 * 安全に使用できるPWMデューティ最大値を計算します。
 * 特定の分解能でのオーバーフローを防止します。
 * @return 安全な最大デューティ値
 */
int pwmSafeMaxDuty() {
    int maxDuty = pwmMaxDuty();
    if (PWM_RES >= 14 && maxDuty > 0) {
        return maxDuty - 1;
    }
    return maxDuty;
}

/**
 * 分解能変更時にデューティ値を新しいスケールに変換します。
 * @param duty 現在のデューティ
 * @param oldMax 以前の最大値
 * @param newMax 新しい最大値
 * @return スケーリング後のデューティ値
 */
int scaleDuty(int duty, int oldMax, int newMax) {
    if (oldMax <= 0) return 0;
    return (static_cast<long>(duty) * newMax) / oldMax;
}

/**
 * 文字列からIPAddress構造体へ変換を試みます。
 * @param s IPアドレス文字列
 * @param out 出力先IPAddress
 * @return 変換に成功したか
 */
bool parseIp(const String& s, IPAddress& out) {
    return out.fromString(s);
}

/**
 * 文字列を厳密に整数へ変換します。
 * 非数値文字が含まれる場合や、intの範囲外の場合は false を返します。
 * @param s 変換対象文字列
 * @param out 出力先数値
 * @return 変換に成功したか
 */
bool parseStrictInt(const String& s, int& out) {
    if (s.length() == 0) return false;

    int sign = 1;
    size_t idx = 0;
    if (s[0] == '+') {
        idx = 1;
    } else if (s[0] == '-') {
        sign = -1;
        idx = 1;
    }
    if (idx >= s.length()) return false;

    long long acc = 0;
    const long long maxAbs = (sign > 0) ? static_cast<long long>(INT_MAX)
                                         : -static_cast<long long>(INT_MIN);

    for (; idx < s.length(); idx++) {
        char c = s[idx];
        if (c < '0' || c > '9') return false;

        acc = (acc * 10) + (c - '0');
        if (acc > maxAbs) return false;
    }

    long long signedValue = (sign > 0) ? acc : -acc;
    if (signedValue < INT_MIN || signedValue > INT_MAX) return false;
    out = static_cast<int>(signedValue);
    return true;
}

/**
 * IPAddressを文字列形式に変換します。
 * @param ip IPAddressオブジェクト
 * @return "192.168.1.1" 形式の文字列
 */
String ipToString(const IPAddress& ip) {
    return ip.toString();
}

/**
 * HTMLタグ等の特殊文字をエスケープします。
 * @param s 対象文字列
 * @return エスケープ済み文字列
 */
String htmlEscape(const String& s) {
    String out;
    out.reserve(s.length() + 16);
    for (size_t i = 0; i < s.length(); i++) {
        char c = s[i];
        if (c == '&') out += "&amp;";
        else if (c == '<') out += "&lt;";
        else if (c == '>') out += "&gt;";
        else if (c == '"') out += "&quot;";
        else if (c == '\'') out += "&#39;";
        else out += c;
    }
    return out;
}

/**
 * WiFi設定をNVS（非揮発性メモリ）に保存します。
 */
void saveWifiConfig() {
    prefs.begin(PREF_NS, false);
    prefs.putString("w_ssid", wifiCfg.ssid);
    prefs.putString("w_pass", wifiCfg.pass);
    prefs.putBool("w_static", wifiCfg.useStatic);
    prefs.putString("w_ip", ipToString(wifiCfg.ip));
    prefs.putString("w_gw", ipToString(wifiCfg.gateway));
    prefs.putString("w_sub", ipToString(wifiCfg.subnet));
    prefs.end();
}

/**
 * WiFi設定をNVSから読み込みます。
 */
void loadWifiConfig() {
    prefs.begin(PREF_NS, true);
    wifiCfg.ssid = prefs.getString("w_ssid", "");
    wifiCfg.pass = prefs.getString("w_pass", "");
    wifiCfg.useStatic = prefs.getBool("w_static", DEFAULT_WIFI_STATIC);
    String ipStr = prefs.getString("w_ip", DEFAULT_WIFI_IP);
    String gwStr = prefs.getString("w_gw", DEFAULT_WIFI_GATEWAY);
    String subStr = prefs.getString("w_sub", DEFAULT_WIFI_SUBNET);
    prefs.end();

    if (!parseIp(ipStr, wifiCfg.ip)) wifiCfg.ip.fromString(DEFAULT_WIFI_IP);
    if (!parseIp(gwStr, wifiCfg.gateway)) wifiCfg.gateway.fromString(DEFAULT_WIFI_GATEWAY);
    if (!parseIp(subStr, wifiCfg.subnet)) wifiCfg.subnet.fromString(DEFAULT_WIFI_SUBNET);
}

// ------------------------------------------------------------
// RGB LED (NeoPixel) 制御部
// ------------------------------------------------------------

/**
 * NeoPixelの色を設定します。
 * @param r 赤 (0-255)
 * @param g 緑 (0-255)
 * @param b 青 (0-255)
 * @param br 輝度 (0-255, 0の場合は現在の輝度を維持)
 */
void setLedColor(uint8_t r, uint8_t g, uint8_t b, uint8_t br = 0) {
    if (br > 0) statusLed.setBrightness(br);
    statusLed.setPixelColor(0, statusLed.Color(r, g, b));
    statusLed.show();
}

/**
 * LEDの状態（色と輝度）をNeoPixelに適用します。
 */
void applyLedState() {
    statusLed.setBrightness(currentBrightness);
    statusLed.setPixelColor(0, statusLed.Color(ledOn ? currentR : 0, 
                                             ledOn ? currentG : 0, 
                                             ledOn ? currentB : 0));
    statusLed.show();
}

/**
 * 通常動作中にBOOTボタンが長押し（5秒）されたかを監視し、
 * 条件を満たせば設定を初期化して再起動します。
 */
void handleFactoryResetButton() {
    static unsigned long pressStart = 0;
    static bool isPressing = false;
    const unsigned long RESET_HOLD_TIME = 5000; // 5秒間

    if (digitalRead(BOOT_BUTTON_PIN) == LOW) {
        if (!isPressing) {
            isPressing = true;
            pressStart = millis();
        } else {
            unsigned long duration = millis() - pressStart;
            if (duration > RESET_HOLD_TIME) {
                // --- リセット実行 ---
                // ユーザーに知らせるためにLEDを赤く高速点滅させる
                for(int i=0; i<10; i++){
                    setLedColor(255, 0, 0, 255); delay(100);
                    setLedColor(0, 0, 0, 0); delay(100);
                }
                prefs.begin(PREF_NS, false);
                prefs.clear();
                prefs.end();
                ESP.restart();
            }
        }
    } else {
        isPressing = false;
    }
}

/**
 * 設定用のアクセスポイント（AP）モードを起動します。
 */
void startConfigAp() { WiFi.mode(WIFI_AP_STA); WiFi.softAP(AP_SSID, AP_PASS); }

/**
 * 保存された設定を使用してWiFi（STAモード）に接続します。
 */
void connectStaFromConfig() {
    if (wifiCfg.ssid.length() == 0) {
        return;
    }

    if (wifiCfg.useStatic) {
        if (!WiFi.config(wifiCfg.ip, wifiCfg.gateway, wifiCfg.subnet)) {
            Serial.println("Failed to apply static IP config");
        }
    } else {
        WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
    }

    WiFi.setAutoReconnect(true); // 自動再接続を有効化
    WiFi.begin(wifiCfg.ssid.c_str(), wifiCfg.pass.c_str());
}

/**
 * 標準的なエラー応答JSONオブジェクトを構築します。
 * @param res 出力先JsonDocument
 * @param cmd 実行されたコマンド名
 * @param code エラーコード
 * @param detail 詳細メッセージ
 */
void buildError(JsonDocument& res, const char* cmd, const char* code, const char* detail) {
    res.clear();
    res["status"] = "error";
    res["cmd"] = cmd;
    res["code"] = code;
    res["detail"] = detail;
}

/**
 * PWMの周波数と分解能の設定を変更し、ハードウェアに反映します。
 * @param freq 周波数 (Hz)
 * @param res 分解能 (bit)
 * @param out レスポンス用JsonDocument
 * @param cmd コマンド名
 * @return 成功したか
 */
bool applyPwmConfigCommand(int freq, int res, JsonDocument& out, const char* cmd) {
    if (freq < 1 || freq > 20000) {
        buildError(out, cmd, "ERR_INVALID_FREQ", "freq must be 1-20000");
        return false;
    }

    if (res < 1 || res > 14) {
        buildError(out, cmd, "ERR_INVALID_RES", "res must be 1-14");
        return false;
    }


    int oldFreq = PWM_FREQ;
    int oldRes  = PWM_RES;
    int oldMax = pwmMaxDuty();
    int oldDuty[PWM_COUNT];
    for (int i = 0; i < PWM_COUNT; i++) oldDuty[i] = PWM_DUTY[i];

    PWM_FREQ = freq;
    PWM_RES  = res;

    int newMax = pwmMaxDuty();
    for (int i = 0; i < PWM_COUNT; i++) {
        PWM_DUTY[i] = scaleDuty(PWM_DUTY[i], oldMax, newMax);
    }

    if (!applyPWMConfig(false)) {
        PWM_FREQ = oldFreq;
        PWM_RES  = oldRes;
        for (int i = 0; i < PWM_COUNT; i++) {
            PWM_DUTY[i] = oldDuty[i];
        }
        applyPWMConfig(false);
        buildError(out, cmd, "ERR_PWM_ATTACH", "failed to apply PWM frequency/resolution");
        return false;
    }

    savePWMFreq(freq);
    savePWMRes(res);

    out["status"] = "ok";
    out["freq"] = freq;
    out["res"]  = res;
    out["max_duty"] = pwmSafeMaxDuty();
    return true;
}

/**
 * JSON形式のコマンドを解析し、適切な処理を実行します（シリアル/HTTP共有）。
 * @param req リクエストJsonVariant
 * @param res レスポンスJsonDocument
 */
void processCommand(JsonVariantConst req, JsonDocument& res) {
    if (!req.containsKey("cmd")) {
        buildError(res, "unknown", "ERR_MISSING_CMD", "cmd field is required");
        return;
    }
    if (!req["cmd"].is<const char*>()) {
        buildError(res, "unknown", "ERR_INVALID_CMD_TYPE", "cmd must be a string");
        return;
    }

    const char* cmd = req["cmd"];

    // RGB LED制御（マニュアル設定）
    if (strcmp(cmd, "set_rgb") == 0) {
        if (!req.containsKey("r") || !req.containsKey("g") || !req.containsKey("b")) {
            buildError(res, cmd, "ERR_MISSING_PARAM", "r, g, b are required");
            return;
        }
        currentR = req["r"];
        currentG = req["g"];
        currentB = req["b"];
        if (req.containsKey("brightness")) currentBrightness = req["brightness"];
        ledOn = true; // LEDをオン状態にする
        applyLedState(); // 適用
        res["status"] = "ok";
        res["r"] = currentR;
        res["g"] = currentG;
        res["b"] = currentB;
        res["brightness"] = currentBrightness;
        return;
    }

    // RGB LED消灯（ステータス表示モード復帰）
    if (strcmp(cmd, "led_off") == 0) {
        ledOn = false;
        applyLedState(); // 消灯を適用
        res["status"] = "ok";
        return;
    }

    // 現在のLED状態取得
    if (strcmp(cmd, "get_led_state") == 0) {
        res["status"] = "ok";
        res["on"] = ledOn;
        res["r"] = currentR; res["g"] = currentG; res["b"] = currentB;
        res["brightness"] = currentBrightness;
        return;
    }

    // 利用可能なコマンド一覧
    if (strcmp(cmd, "help") == 0) {
        res["status"] = "ok";
        JsonArray arr = res.createNestedArray("commands");
        arr.add("read_di");
        arr.add("set_do");
        arr.add("read_adc");
        arr.add("set_pwm");
        arr.add("get_status");
        arr.add("get_io_state");
        arr.add("get_pwm_config");
        arr.add("set_pwm_config");
        arr.add("set_rgb");
        arr.add("led_off");
        arr.add("get_led_state");
        arr.add("ping");
        arr.add("help");
        return;
    }

    // デバイス稼働状況取得
    if (strcmp(cmd, "get_status") == 0) {
        res["status"] = "ok";
        res["uptime_ms"] = millis();
        res["free_heap"] = esp_get_free_heap_size();
        res["wifi_connected"] = (WiFi.status() == WL_CONNECTED);
        res["wifi_ip"] = WiFi.localIP().toString();
        res["ap_ip"] = WiFi.softAPIP().toString();
        return;
    }

    // 全ての入出力状態を一括取得
    if (strcmp(cmd, "get_io_state") == 0) {
        res["status"] = "ok";

        JsonArray di = res.createNestedArray("dio_in");
        for (int i = 0; i < DIO_IN_COUNT; i++) di.add(digitalRead(DIO_IN_PINS[i]));

        JsonArray doo = res.createNestedArray("dio_out");
        for (int i = 0; i < DIO_OUT_COUNT; i++) doo.add(digitalRead(DIO_OUT_PINS[i]));

        JsonArray adc = res.createNestedArray("adc");
        for (int i = 0; i < ADC_COUNT; i++) adc.add(readADC(ADC_PINS[i]));

        JsonArray pwm = res.createNestedArray("pwm");
        for (int i = 0; i < PWM_COUNT; i++) pwm.add(PWM_DUTY[i]);
        return;
    }

    // 指定したピンのデジタル入力読み取り
    if (strcmp(cmd, "read_di") == 0) {
        if (!req.containsKey("pin_id")) {
            buildError(res, cmd, "ERR_MISSING_PARAM", "pin_id is required");
            return;
        }
        if (!req["pin_id"].is<int>()) {
            buildError(res, cmd, "ERR_INVALID_PARAM_TYPE", "pin_id must be an integer");
            return;
        }

        int id = req["pin_id"];
        if (!checkRange(id, DIO_IN_COUNT)) {
            char errMsg[48];
            snprintf(errMsg, sizeof(errMsg), "pin_id must be 0-%d", DIO_IN_COUNT-1);
            buildError(res, cmd, "ERR_INVALID_DIO_IN_PIN_ID", errMsg);
            return;
        }

        res["status"] = "ok";
        res["value"] = digitalRead(DIO_IN_PINS[id]);
        return;
    }

    // 指定したピンのデジタル出力設定
    if (strcmp(cmd, "set_do") == 0) {
        if (!req.containsKey("pin_id") || !req.containsKey("value")) {
            buildError(res, cmd, "ERR_MISSING_PARAM", "pin_id and value are required");
            return;
        }
        if (!req["pin_id"].is<int>() || !req["value"].is<int>()) {
            buildError(res, cmd, "ERR_INVALID_PARAM_TYPE", "pin_id and value must be integers");
            return;
        }

        int id = req["pin_id"];
        int value = req["value"];

        if (!checkRange(id, DIO_OUT_COUNT)) {
            char errMsg[48];
            snprintf(errMsg, sizeof(errMsg), "pin_id must be 0-%d", DIO_OUT_COUNT-1);
            buildError(res, cmd, "ERR_INVALID_DIO_OUT_PIN_ID", errMsg);
            return;
        }
        if (!(value == 0 || value == 1)) {
            buildError(res, cmd, "ERR_INVALID_VALUE", "value must be 0 or 1");
            return;
        }

        digitalWrite(DIO_OUT_PINS[id], value ? HIGH : LOW);
        res["status"] = "ok";
        return;
    }

    // 指定したピンのアナログ入力読み取り
    if (strcmp(cmd, "read_adc") == 0) {
        if (!req.containsKey("pin_id")) {
            buildError(res, cmd, "ERR_MISSING_PARAM", "pin_id is required");
            return;
        }
        if (!req["pin_id"].is<int>()) {
            buildError(res, cmd, "ERR_INVALID_PARAM_TYPE", "pin_id must be an integer");
            return;
        }

        int id = req["pin_id"];
        if (!checkRange(id, ADC_COUNT)) {
            char errMsg[48];
            snprintf(errMsg, sizeof(errMsg), "pin_id must be 0-%d", ADC_COUNT-1);
            buildError(res, cmd, "ERR_INVALID_ADC_PIN_ID", errMsg);
            return;
        }

        res["status"] = "ok";
        res["value"] = readADC(ADC_PINS[id]);
        return;
    }

    // 指定したピンのPWM出力設定
    if (strcmp(cmd, "set_pwm") == 0) {
        if (!req.containsKey("pin_id") || !req.containsKey("duty")) {
            buildError(res, cmd, "ERR_MISSING_PARAM", "pin_id and duty are required");
            return;
        }
        if (!req["pin_id"].is<int>() || !req["duty"].is<int>()) {
            buildError(res, cmd, "ERR_INVALID_PARAM_TYPE", "pin_id and duty must be integers");
            return;
        }

        int id = req["pin_id"];
        int duty = req["duty"];

        if (!checkRange(id, PWM_COUNT)) {
            char errMsg[48];
            snprintf(errMsg, sizeof(errMsg), "pin_id must be 0-%d", PWM_COUNT-1);
            buildError(res, cmd, "ERR_INVALID_PWM_PIN_ID", errMsg);
            return;
        }

        duty = constrain(duty, 0, pwmSafeMaxDuty());
        ledcWrite(PWM_PINS[id], duty);
        PWM_DUTY[id] = duty;

        res["status"] = "ok";
        res["duty"] = duty;
        res["max_duty"] = pwmSafeMaxDuty();
        return;
    }

    // PWM共通設定の取得
    if (strcmp(cmd, "get_pwm_config") == 0) {
        res["status"] = "ok";
        res["freq"] = PWM_FREQ;
        res["res"] = PWM_RES;
        return;
    }

    // PWM共通設定の変更
    if (strcmp(cmd, "set_pwm_config") == 0) {
        if (!req.containsKey("freq") || !req.containsKey("res")) {
            buildError(res, cmd, "ERR_MISSING_PARAM", "freq and res are required");
            return;
        }
        if (!req["freq"].is<int>() || !req["res"].is<int>()) {
            buildError(res, cmd, "ERR_INVALID_PARAM_TYPE", "freq and res must be integers");
            return;
        }

        int freq = req["freq"];
        int pwmRes = req["res"];
        applyPwmConfigCommand(freq, pwmRes, res, cmd);
        return;
    }

    // 生存確認
    if (strcmp(cmd, "ping") == 0) {
        res["status"] = "ok";
        res["message"] = "pong";
        return;
    }

    buildError(res, cmd, "ERR_UNKNOWN_COMMAND", "command not recognized");
}

/**
 * JSONドキュメントをシリアルポート経由で送信します。
 * @param doc 送信するJsonDocument
 */
void sendJsonToSerial(JsonDocument& doc) {
    serializeJson(doc, Serial);
    Serial.println();
}

/**
 * HTTPリクエスト（/api）を解析し、JSONまたはクエリパラメータからコマンドを処理します。
 * POST JSON または GET クエリの両方に対応します。
 */
void handleApi() {
    StaticJsonDocument<1024> req;
    StaticJsonDocument<1024> res;

    if (server.method() == HTTP_POST && server.hasArg("plain")) {
        DeserializationError err = deserializeJson(req, server.arg("plain"));
        if (err) {
            buildError(res, "unknown", "ERR_JSON_PARSE", err.c_str());
            String out;
            serializeJson(res, out);
            server.send(400, "application/json", out);
            return;
        }
    } else {
        req["cmd"] = server.arg("cmd");
        
        // ヘルパーラムダでクエリパラメータのパースとバリデーションを共通化
        auto parseParam = [&](const String& argName, const char* key) -> bool {
            if (server.hasArg(argName)) {
                int val;
                if (!parseStrictInt(server.arg(argName), val)) {
                    buildError(res, "unknown", "ERR_INVALID_PARAM_TYPE", (argName + " must be an integer").c_str());
                    String out;
                    serializeJson(res, out);
                    server.send(400, "application/json", out);
                    return false;
                }
                req[key] = val;
            }
            return true;
        };
        
        // 各パラメータをパースし、エラーがあれば即座にリターン
        if (!parseParam("pin_id", "pin_id")) return;
        if (!parseParam("value", "value")) return;
        if (!parseParam("duty", "duty")) return;
        if (!parseParam("freq", "freq")) return;
        if (!parseParam("res", "res")) return;
        if (!parseParam("r", "r")) return;
        if (!parseParam("g", "g")) return;
        if (!parseParam("b", "b")) return;
        if (!parseParam("brightness", "brightness")) return;
    }

    processCommand(req.as<JsonVariantConst>(), res);

    // API処理後の応答送信
    {
        String out;
        serializeJson(res, out);
        int code = (String((const char*)res["status"]) == "ok") ? 200 : 400;
        server.send(code, "application/json", out);
        return;
    }
}

/**
 * WiFi設定画面のHTML生成を行います。
 * @return HTML文字列
 */
String makeConfigPage() {
    String modeDhcpChecked = wifiCfg.useStatic ? "" : "checked";
    String modeStaticChecked = wifiCfg.useStatic ? "checked" : "";
    String staIp = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : "not connected";
    String escSsid = htmlEscape(wifiCfg.ssid);
    String escStaIp = htmlEscape(staIp);
    String escApSsid = htmlEscape(String(AP_SSID));

    String html;
    html.reserve(2048); // メモリ割り当て回数を減らす
    html += "<!doctype html><html><head><meta charset='utf-8'>";
    html += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
    html += "<title>ESP32 S3 Network Setup</title>";
    html += "<style>body{font-family:sans-serif;max-width:760px;margin:20px auto;padding:0 12px;}";
    html += "label{display:block;margin-top:10px;font-weight:600;}input{width:100%;padding:8px;margin-top:4px;box-sizing:border-box;}";
    html += "fieldset{margin-top:12px;padding:12px;}button{margin-top:14px;padding:10px 16px;}";
    html += "code{background:#f1f1f1;padding:2px 5px;border-radius:4px;}</style></head><body>";
    html += "<h2>ESP32 S3 Network Setup</h2>";
    html += "<p>AP SSID: <code>" + escApSsid + "</code></p>";
    html += "<p>AP IP: <code>" + WiFi.softAPIP().toString() + "</code></p>";
    html += "<p>STA status: <code>" + escStaIp + "</code></p>";
    html += "<form method='post' action='/save'>";
    html += "<label>WiFi SSID</label><input name='ssid' value='" + escSsid + "' required>";
    html += "<label>WiFi Password</label><input name='pass' type='password' value=''>";
    html += "<fieldset><legend>IP Mode</legend>";
    html += "<label><input type='radio' name='ip_mode' value='dhcp' " + modeDhcpChecked + "> DHCP</label>";
    html += "<label><input type='radio' name='ip_mode' value='static' " + modeStaticChecked + "> Static</label>";
    html += "</fieldset>";
    html += "<label>Static IP</label><input name='ip' value='" + ipToString(wifiCfg.ip) + "' placeholder='192.168.1.50'>";
    html += "<label>Gateway</label><input name='gateway' value='" + ipToString(wifiCfg.gateway) + "' placeholder='192.168.1.1'>";
    html += "<label>Subnet</label><input name='subnet' value='" + ipToString(wifiCfg.subnet) + "' placeholder='255.255.255.0'>";
    html += "<button type='submit'>Save And Connect</button>";
    html += "</form>";
    html += "<p>HTTP API: <code>/api</code> (GET/POST JSON)</p>";
    html += "</body></html>";
    return html;
}

/**
 * ルートパス（/）へのアクセスを処理します。
 */
void handleRoot() {
    server.send(200, "text/html", makeConfigPage());
}

/**
 * WebUIからのWiFi設定保存リクエストを処理します。
 */
void handleSave() {
    if (!server.hasArg("ssid")) {
        server.send(400, "text/plain", "ssid is required");
        return;
    }

    String newSsid = server.arg("ssid");
    String newPass = server.arg("pass");
    String mode = server.arg("ip_mode");

    if (newSsid.length() == 0) {
        server.send(400, "text/plain", "ssid is required");
        return;
    }

    bool useStatic = (mode == "static");
    IPAddress ip, gw, sub;

    if (useStatic) {
        if (!parseIp(server.arg("ip"), ip) || !parseIp(server.arg("gateway"), gw) || !parseIp(server.arg("subnet"), sub)) {
            server.send(400, "text/plain", "invalid static ip/gateway/subnet");
            return;
        }
    } else {
        ip.fromString(DEFAULT_WIFI_IP);
        gw.fromString(DEFAULT_WIFI_GATEWAY);
        sub.fromString(DEFAULT_WIFI_SUBNET);
    }

    wifiCfg.ssid = newSsid;
    wifiCfg.pass = newPass;
    wifiCfg.useStatic = useStatic;
    wifiCfg.ip = ip;
    wifiCfg.gateway = gw;
    wifiCfg.subnet = sub;
    saveWifiConfig();

    String html = "<html><body><h3>Saved</h3>";
    html += "<p>WiFi settings saved. Device will restart and try to connect.</p>";
    html += "</body></html>";
    server.send(200, "text/html", html);

    delay(500);
    ESP.restart();
}

/**
 * WiFiの接続状態を監視し、状況をシリアルに出力します。
 * WiFi.begin() は一度呼べば内部で自動リトライされるため、ここでは状態監視のみを行います。
 */
void manageWiFiConnection() {
    static unsigned long lastCheck = 0;
    if (millis() - lastCheck < 2000) return; 
    lastCheck = millis();

    if (wifiCfg.ssid.length() == 0) return;

    wl_status_t status = WiFi.status();
    StaticJsonDocument<256> event;

    if (status == WL_CONNECTED) {
        if (!wifiConnected) {
            wifiConnected = true;
            event["type"] = "wifi_event";
            event["status"] = "connected";
            sendJsonToSerial(event);
        }
        // 接続試行中のドット表示はシリアルAPIを破壊するため廃止
        // 必要であれば {"status": "connecting"} を送る
    }
}

/**
 * Webサーバの起動とルーティング設定を行います。
 */
void setupWebServer() {
    server.on("/", HTTP_GET, handleRoot);
    server.on("/save", HTTP_POST, handleSave);
    server.on("/api", HTTP_ANY, handleApi);
    server.begin();
}

/**
 * 現在のPWM設定をハードウェアピンに反映させます。
 * @param resetDuty すべての出力を0にするか
 * @return すべてのピンが正常に設定されたか
 */
bool applyPWMConfig(bool resetDuty) {
    int maxDuty = pwmSafeMaxDuty();
    bool okAll = true;

    for (int i = 0; i < PWM_COUNT; i++) {
        ledcDetach(PWM_PINS[i]);
        bool ok = ledcAttach(PWM_PINS[i], PWM_FREQ, PWM_RES);
        if (!ok) {
            okAll = false;
            continue;
        }

        if (resetDuty) PWM_DUTY[i] = 0;
        PWM_DUTY[i] = constrain(PWM_DUTY[i], 0, maxDuty);
        ledcWrite(PWM_PINS[i], PWM_DUTY[i]);
    }

    return okAll;
}

// ------------------------------------------------------------
// PIN_ID 範囲チェック
// ------------------------------------------------------------
bool checkRange(int id, int max) {
    // ------------------------------------------------------------
    // 【ID範囲チェック】
    // ------------------------------------------------------------
    return (id >= 0 && id < max);
}

// ------------------------------------------------------------
// ADC 平均化
// ------------------------------------------------------------
int readADC(int gpio) {
    // ------------------------------------------------------------
    // 【ADC値平均化取得】
    // ------------------------------------------------------------
    long sum = 0; // オーバーフロー防止のためlongを使用
    for (int i = 0; i < ADC_SAMPLES; i++) {
        sum += analogRead(gpio);
    }
    return static_cast<int>(sum / ADC_SAMPLES);
}

/**
 * シリアル通信向けにエラーメッセージをJSONで送信します。
 * @param cmd コマンド名
 * @param code エラーコード
 * @param detail 詳細
 */
void sendError(const char* cmd, const char* code, const char* detail) {
    StaticJsonDocument<192> res;
    buildError(res, cmd, code, detail);
    sendJsonToSerial(res);
}

// ------------------------------------------------------------
// 非ブロッキング行読み取り
// ------------------------------------------------------------

/**
 * シリアルポートから1行受信を試みます。非ブロッキング動作です。
 * @param out 受信文字列の格納先
 * @param overflowed バッファオーバーフローが発生したか
 * @return 1行受信が完了したか
 */
bool readLine(String& out, bool& overflowed) {
    tud_task();
    delay(1);

    static String buf;
    static bool dropping = false;
    const size_t MAX_LINE_LEN = 512;
    overflowed = false;

    while (Serial.available()) {
        char c = Serial.read();

        if (dropping) {
            if (c == '\n') {
                dropping = false;
                buf = "";
            }
            continue;
        }

        if (c == '\r') continue;

        if (c == '\n') {
            out = buf;
            buf = "";
            return true;
        }

        if (buf.length() >= MAX_LINE_LEN) {
            buf = "";
            dropping = true;
            overflowed = true;
            return false;
        }

        buf += c;
    }
    return false;
}

// ------------------------------------------------------------
// PWM設定のNVS管理
// ------------------------------------------------------------

/**
 * NVSからPWM周波数を取得します。
 * @return 保存されている周波数
 */
int loadPWMFreq() {
    prefs.begin(PREF_NS, true);
    int v = prefs.getInt("pwm_freq", DEFAULT_PWM_FREQ);
    prefs.end();
    return v;
}

/**
 * NVSからPWM分解能を取得します。
 * @return 保存されている分解能
 */
int loadPWMRes() {
    prefs.begin(PREF_NS, true);
    int v = prefs.getInt("pwm_res", DEFAULT_PWM_RES);
    prefs.end();
    return v;
}

/**
 * PWM周波数をNVSに保存します。
 * @param v 保存する周波数
 */
void savePWMFreq(int v) {
    prefs.begin(PREF_NS, false);
    prefs.putInt("pwm_freq", v);
    prefs.end();
}

/**
 * PWM分解能をNVSに保存します。
 * @param v 保存する分解能
 */
void savePWMRes(int v) {
    prefs.begin(PREF_NS, false);
    prefs.putInt("pwm_res", v);
    prefs.end();
}

// ------------------------------------------------------------
// 初期化
// ------------------------------------------------------------

/**
 * ハードウェアおよびソフトウェアの初期化を行います。
 */
void setup() {
    Serial.begin(115200);
    
    // WiFi設定を反映させる前に、以前の接続状態を完全にクリア
    WiFi.disconnect(true, true);
    delay(500); 

    PWM_FREQ = loadPWMFreq();
    PWM_RES  = loadPWMRes();
    loadWifiConfig();

    pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
    for (int i = 0; i < DIO_IN_COUNT; i++) {
        pinMode(DIO_IN_PINS[i], INPUT);
    }
    for (int i = 0; i < DIO_OUT_COUNT; i++) {
        pinMode(DIO_OUT_PINS[i], OUTPUT);
        digitalWrite(DIO_OUT_PINS[i], LOW);
    }

    applyPWMConfig(true);

    statusLed.begin();

    startConfigAp();
    connectStaFromConfig();
    setupWebServer();
}

// ------------------------------------------------------------
// メインループ
// ------------------------------------------------------------

/**
 * Webサーバ処理、LED更新、シリアルコマンド処理を回します。
 */
void loop() {
    yield();
    server.handleClient();
    handleFactoryResetButton();
    manageWiFiConnection(); // WiFiの状態を監視

    String line;
    bool overflowed = false;

    if (!readLine(line, overflowed)) {
        if (overflowed) {
            sendError("unknown", "ERR_LINE_TOO_LONG", "input line too long");
        }
        return;
    }

    line.trim();
    if (line.length() == 0) return;

    StaticJsonDocument<512> doc;
    auto err = deserializeJson(doc, line);
    if (err) {
        sendError("unknown", "ERR_JSON_PARSE", err.c_str());
        return;
    }

    StaticJsonDocument<512> res;
    processCommand(doc.as<JsonVariantConst>(), res);
    sendJsonToSerial(res);
}
