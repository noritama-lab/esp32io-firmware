// ============================================================
// ESP32 S3 IO DEVICE WIFI メインスケッチ
// - デジタル入出力、アナログ入力、PWM出力、WiFi/AP設定、Webサーバ/API制御
// - ピン数可変対応、設定保存、シリアル/HTTP制御
// ============================================================

// ------------------------------------------------------------
// 【ピン配列・カウントマクロ】
// ------------------------------------------------------------
// DIO_IN_PINS: デジタル入力ピン番号配列
// DIO_OUT_PINS: デジタル出力ピン番号配列
// ADC_PINS: アナログ入力ピン番号配列
// PWM_PINS: PWM出力ピン番号配列
// *_COUNT: 各配列の要素数（ピン数）
// ------------------------------------------------------------
// 【WiFi/AP/設定用定数】
// ------------------------------------------------------------
// PREF_NS: NVS保存用ネームスペース
// AP_SSID/AP_PASS: 設定用APのSSID/パスワード
// DEFAULT_WIFI_*: WiFi静的IP設定のデフォルト値
// ------------------------------------------------------------
// 【PWMデフォルト設定】
// ------------------------------------------------------------
// PWMのデフォルト周波数・分解能
// ------------------------------------------------------------
// 【グローバル変数】
// ------------------------------------------------------------
// PWM_FREQ/PWM_RES: 現在のPWM設定値
// PWM_DUTY: 各PWMピンのデューティ配列
// prefs: NVSアクセス用
// server: Webサーバインスタンス
// ------------------------------------------------------------
// 【WiFi設定構造体】
// ------------------------------------------------------------
// WiFi/AP/静的IP等の設定を保持
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WebServer.h>
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

// --- カウントマクロ ---
#define DIO_IN_COUNT   (sizeof(DIO_IN_PINS)/sizeof(DIO_IN_PINS[0]))
#define DIO_OUT_COUNT  (sizeof(DIO_OUT_PINS)/sizeof(DIO_OUT_PINS[0]))
#define ADC_COUNT      (sizeof(ADC_PINS)/sizeof(ADC_PINS[0]))
#define PWM_COUNT      (sizeof(PWM_PINS)/sizeof(PWM_PINS[0]))

// --- WiFi/AP/設定用定数 ---
const char* PREF_NS           = "esp32io";
const char* AP_SSID           = "ESP32_S3_IO_SETUP";
const char* AP_PASS           = "esp32setup";
const bool  DEFAULT_WIFI_STATIC = false;
const char* DEFAULT_WIFI_IP     = "192.168.1.50";
const char* DEFAULT_WIFI_GATEWAY= "192.168.1.1";
const char* DEFAULT_WIFI_SUBNET = "255.255.255.0";

// --- PWMデフォルト ---
const int DEFAULT_PWM_FREQ = 5000;
const int DEFAULT_PWM_RES  = 8;

// ------------------------------------------------------------
// グローバル変数
// ------------------------------------------------------------
int PWM_FREQ = DEFAULT_PWM_FREQ;
int PWM_RES  = DEFAULT_PWM_RES;
int PWM_DUTY[PWM_COUNT] = {0};

Preferences prefs;
WebServer server(80);

// ------------------------------------------------------------
// 構造体定義
// ------------------------------------------------------------
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

int pwmMaxDuty() {
    // ------------------------------------------------------------
    // 【PWMデューティ最大値計算】
    // ------------------------------------------------------------
    return (1UL << PWM_RES) - 1;
}

int pwmSafeMaxDuty() {
    // ------------------------------------------------------------
    // 【PWMデューティ最大値（安全値）計算】
    // 分解能14bit時の最大値調整
    // ------------------------------------------------------------
    int maxDuty = pwmMaxDuty();
    if (PWM_RES >= 14 && maxDuty > 0) {
        return maxDuty - 1;
    }
    return maxDuty;
}

int scaleDuty(int duty, int oldMax, int newMax) {
    // ------------------------------------------------------------
    // 【PWMデューティ値スケーリング】
    // 分解能変更時の値変換
    // ------------------------------------------------------------
    if (oldMax <= 0) return 0;
    return (static_cast<long>(duty) * newMax) / oldMax;
}

bool parseIp(const String& s, IPAddress& out) {
    // ------------------------------------------------------------
    // 【IPアドレス文字列→IPAddress変換】
    // ------------------------------------------------------------
    return out.fromString(s);
}

bool parseStrictInt(const String& s, int& out) {
    // ------------------------------------------------------------
    // 【厳密な整数変換（文字列→int）】
    // ------------------------------------------------------------
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

String ipToString(const IPAddress& ip) {
    // ------------------------------------------------------------
    // 【IPAddress→文字列変換】
    // ------------------------------------------------------------
    return ip.toString();
}

String htmlEscape(const String& s) {
    // ------------------------------------------------------------
    // 【HTMLエスケープ】
    // ------------------------------------------------------------
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

void saveWifiConfig() {
    // ------------------------------------------------------------
    // 【WiFi設定をNVS保存】
    // ------------------------------------------------------------
    prefs.begin(PREF_NS, false);
    prefs.putString("w_ssid", wifiCfg.ssid);
    prefs.putString("w_pass", wifiCfg.pass);
    prefs.putBool("w_static", wifiCfg.useStatic);
    prefs.putString("w_ip", ipToString(wifiCfg.ip));
    prefs.putString("w_gw", ipToString(wifiCfg.gateway));
    prefs.putString("w_sub", ipToString(wifiCfg.subnet));
    prefs.end();
}

void loadWifiConfig() {
    // ------------------------------------------------------------
    // 【WiFi設定をNVSから読み出し】
    // ------------------------------------------------------------
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

void clearAllSettingsIfBootLongPressed() {
    // ------------------------------------------------------------
    // 【BOOT長押しで全設定クリア】
    // ------------------------------------------------------------
    const unsigned long HOLD_MS = 2000;
    pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
    delay(20);

    if (digitalRead(BOOT_BUTTON_PIN) != LOW) return;

    unsigned long start = millis();
    while (millis() - start < HOLD_MS) {
        if (digitalRead(BOOT_BUTTON_PIN) != LOW) {
            Serial.println("BOOT button released before long-press threshold");
            return;
        }
        delay(10);
    }

    prefs.begin(PREF_NS, false);
    prefs.clear();
    prefs.end();
    Serial.println("BOOT long-press detected: cleared all saved settings");
}

void startConfigAp() {
    // ------------------------------------------------------------
    // 【設定用AP起動】
    // ------------------------------------------------------------
    WiFi.mode(WIFI_AP_STA);
    bool ok = WiFi.softAP(AP_SSID, AP_PASS);
    Serial.print("AP SSID: ");
    Serial.println(AP_SSID);
    Serial.print("AP start: ");
    Serial.println(ok ? "ok" : "failed");
    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());
}

bool connectStaFromConfig() {
    // ------------------------------------------------------------
    // 【保存済みWiFi設定でSTA接続】
    // ------------------------------------------------------------
    if (wifiCfg.ssid.length() == 0) {
        wifiConnected = false;
        return false;
    }

    if (wifiCfg.useStatic) {
        if (!WiFi.config(wifiCfg.ip, wifiCfg.gateway, wifiCfg.subnet)) {
            Serial.println("Failed to apply static IP config");
            wifiConnected = false;
            return false;
        }
    } else {
        WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
    }

    WiFi.begin(wifiCfg.ssid.c_str(), wifiCfg.pass.c_str());
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
        delay(250);
    }

    wifiConnected = (WiFi.status() == WL_CONNECTED);
    Serial.print("STA connected: ");
    Serial.println(wifiConnected ? "yes" : "no");
    if (wifiConnected) {
        Serial.print("STA IP: ");
        Serial.println(WiFi.localIP());
    }
    return wifiConnected;
}

void buildError(JsonDocument& res, const char* cmd, const char* code, const char* detail) {
    // ------------------------------------------------------------
    // 【エラー応答JSON生成】
    // ------------------------------------------------------------
    res.clear();
    res["status"] = "error";
    res["cmd"] = cmd;
    res["code"] = code;
    res["detail"] = detail;
}

bool applyPwmConfigCommand(int freq, int res, JsonDocument& out, const char* cmd) {
    // ------------------------------------------------------------
    // 【PWM周波数・分解能変更コマンド処理】
    // ------------------------------------------------------------
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

void processCommand(JsonVariantConst req, JsonDocument& res) {
    // ------------------------------------------------------------
    // 【APIコマンド分岐・処理本体】
    // ------------------------------------------------------------
    if (!req.containsKey("cmd")) {
        buildError(res, "unknown", "ERR_MISSING_CMD", "cmd field is required");
        return;
    }
    if (!req["cmd"].is<const char*>()) {
        buildError(res, "unknown", "ERR_INVALID_CMD_TYPE", "cmd must be a string");
        return;
    }

    const char* cmd = req["cmd"];

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
        arr.add("ping");
        arr.add("help");
        return;
    }

    if (strcmp(cmd, "get_status") == 0) {
        res["status"] = "ok";
        res["uptime_ms"] = millis();
        res["free_heap"] = esp_get_free_heap_size();
        res["wifi_connected"] = (WiFi.status() == WL_CONNECTED);
        res["wifi_ip"] = WiFi.localIP().toString();
        res["ap_ip"] = WiFi.softAPIP().toString();
        return;
    }

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

    if (strcmp(cmd, "get_pwm_config") == 0) {
        res["status"] = "ok";
        res["freq"] = PWM_FREQ;
        res["res"] = PWM_RES;
        return;
    }

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

    if (strcmp(cmd, "ping") == 0) {
        res["status"] = "ok";
        res["message"] = "pong";
        return;
    }

    buildError(res, cmd, "ERR_UNKNOWN_COMMAND", "command not recognized");
}

void sendJsonToSerial(JsonDocument& doc) {
    // ------------------------------------------------------------
    // 【シリアルへJSON送信】
    // ------------------------------------------------------------
    serializeJson(doc, Serial);
    Serial.println();
}

void handleApi() {
    // ------------------------------------------------------------
    // 【/apiエンドポイント処理】
    // ------------------------------------------------------------
    StaticJsonDocument<512> req;
    StaticJsonDocument<512> res;

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
        int parsed = 0;
        if (server.hasArg("pin_id")) {
            if (!parseStrictInt(server.arg("pin_id"), parsed)) {
                buildError(res, "unknown", "ERR_INVALID_PARAM_TYPE", "pin_id must be an integer");
                String out;
                serializeJson(res, out);
                server.send(400, "application/json", out);
                return;
            }
            req["pin_id"] = parsed;
        }
        if (server.hasArg("value")) {
            if (!parseStrictInt(server.arg("value"), parsed)) {
                buildError(res, "unknown", "ERR_INVALID_PARAM_TYPE", "value must be an integer");
                String out;
                serializeJson(res, out);
                server.send(400, "application/json", out);
                return;
            }
            req["value"] = parsed;
        }
        if (server.hasArg("duty")) {
            if (!parseStrictInt(server.arg("duty"), parsed)) {
                buildError(res, "unknown", "ERR_INVALID_PARAM_TYPE", "duty must be an integer");
                String out;
                serializeJson(res, out);
                server.send(400, "application/json", out);
                return;
            }
            req["duty"] = parsed;
        }
        if (server.hasArg("freq")) {
            if (!parseStrictInt(server.arg("freq"), parsed)) {
                buildError(res, "unknown", "ERR_INVALID_PARAM_TYPE", "freq must be an integer");
                String out;
                serializeJson(res, out);
                server.send(400, "application/json", out);
                return;
            }
            req["freq"] = parsed;
        }
        if (server.hasArg("res")) {
            if (!parseStrictInt(server.arg("res"), parsed)) {
                buildError(res, "unknown", "ERR_INVALID_PARAM_TYPE", "res must be an integer");
                String out;
                serializeJson(res, out);
                server.send(400, "application/json", out);
                return;
            }
            req["res"] = parsed;
        }
    }

    processCommand(req.as<JsonVariantConst>(), res);

    String out;
    serializeJson(res, out);
    int code = (String((const char*)res["status"]) == "ok") ? 200 : 400;
    server.send(code, "application/json", out);
}

String makeConfigPage() {
    // ------------------------------------------------------------
    // 【設定用WebページHTML生成】
    // ------------------------------------------------------------
    String modeDhcpChecked = wifiCfg.useStatic ? "" : "checked";
    String modeStaticChecked = wifiCfg.useStatic ? "checked" : "";
    String staIp = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : "not connected";
    String escSsid = htmlEscape(wifiCfg.ssid);
    String escStaIp = htmlEscape(staIp);
    String escApSsid = htmlEscape(String(AP_SSID));

    String html;
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

void handleRoot() {
    // ------------------------------------------------------------
    // 【/ルートエンドポイント処理】
    // ------------------------------------------------------------
    server.send(200, "text/html", makeConfigPage());
}

void handleSave() {
    // ------------------------------------------------------------
    // 【/saveエンドポイント処理】
    // ------------------------------------------------------------
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

    bool ok = connectStaFromConfig();

    String html = "<html><body><h3>Saved</h3>";
    html += String("<p>STA connect: ") + (ok ? "success" : "failed") + "</p>";
    if (ok) {
        html += "<p>Device will restart now.</p>";
    } else {
        html += "<p>Device stays running so you can retry settings.</p>";
    }
    html += "<p><a href='/'>Back</a></p></body></html>";
    server.send(200, "text/html", html);

    if (ok) {
        delay(300);
        ESP.restart();
    }
}

void setupWebServer() {
    // ------------------------------------------------------------
    // 【Webサーバ初期化・ルーティング】
    // ------------------------------------------------------------
    server.on("/", HTTP_GET, handleRoot);
    server.on("/save", HTTP_POST, handleSave);
    server.on("/api", HTTP_ANY, handleApi);
    server.begin();
}

bool applyPWMConfig(bool resetDuty) {
    // ------------------------------------------------------------
    // 【PWM設定反映（attach/detach）】
    // ------------------------------------------------------------
    int maxDuty = pwmSafeMaxDuty();
    bool okAll = true;

    for (int i = 0; i < PWM_COUNT; i++) {
        // 既存の PWM 設定が残っていると res/freq が切り替わらないことがあるため、
        // 一度デタッチしてから再アタッチする。
        ledcDetach(PWM_PINS[i]);
        bool ok = ledcAttach(PWM_PINS[i], PWM_FREQ, PWM_RES);
        if (!ok) {
            okAll = false;
            Serial.printf("PWM attach failed: pin=%d freq=%d res=%d\n", PWM_PINS[i], PWM_FREQ, PWM_RES);
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
    const int N = 8;
    int sum = 0;
    for (int i = 0; i < N; i++) {
        sum += analogRead(gpio);
    }
    return sum / N;
}

// ------------------------------------------------------------
// エラー応答
// ------------------------------------------------------------
void sendError(const char* cmd, const char* code, const char* detail) {
    // ------------------------------------------------------------
    // 【エラー応答をシリアル送信】
    // ------------------------------------------------------------
    StaticJsonDocument<192> res;
    buildError(res, cmd, code, detail);
    sendJsonToSerial(res);
}

// ------------------------------------------------------------
// 非ブロッキング行読み取り
// ------------------------------------------------------------
bool readLine(String& out, bool& overflowed) {
    // USB CDC のタスクを回す（WDT防止）
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
// PWM 設定の保存・読み込み
// ------------------------------------------------------------
int loadPWMFreq() {
    // ------------------------------------------------------------
    // 【PWM周波数をNVSから取得】
    // ------------------------------------------------------------
    prefs.begin(PREF_NS, true);
    int v = prefs.getInt("pwm_freq", DEFAULT_PWM_FREQ);
    prefs.end();
    return v;
}

int loadPWMRes() {
    // ------------------------------------------------------------
    // 【PWM分解能をNVSから取得】
    // ------------------------------------------------------------
    prefs.begin(PREF_NS, true);
    int v = prefs.getInt("pwm_res", DEFAULT_PWM_RES);
    prefs.end();
    return v;
}

void savePWMFreq(int v) {
    // ------------------------------------------------------------
    // 【PWM周波数をNVS保存】
    // ------------------------------------------------------------
    prefs.begin(PREF_NS, false);
    prefs.putInt("pwm_freq", v);
    prefs.end();
}

void savePWMRes(int v) {
    // ------------------------------------------------------------
    // 【PWM分解能をNVS保存】
    // ------------------------------------------------------------
    prefs.begin(PREF_NS, false);
    prefs.putInt("pwm_res", v);
    prefs.end();
}

// ------------------------------------------------------------
// 初期化
// ------------------------------------------------------------
void setup() {
    // ------------------------------------------------------------
    // 【初期化処理】
    // ------------------------------------------------------------
    Serial.begin(115200);
    delay(100);

    clearAllSettingsIfBootLongPressed();

    PWM_FREQ = loadPWMFreq();
    PWM_RES  = loadPWMRes();
    loadWifiConfig();

    for (int i = 0; i < DIO_IN_COUNT; i++) {
        pinMode(DIO_IN_PINS[i], INPUT);
    }
    for (int i = 0; i < DIO_OUT_COUNT; i++) {
        pinMode(DIO_OUT_PINS[i], OUTPUT);
        digitalWrite(DIO_OUT_PINS[i], LOW);
    }

    applyPWMConfig(true);

    startConfigAp();
    connectStaFromConfig();
    setupWebServer();
}

// ------------------------------------------------------------
// メインループ
// ------------------------------------------------------------
void loop() {
    yield();
    server.handleClient();

    // ------------------------------------------------------------
    // 【シリアル行読み取り】
    // ------------------------------------------------------------
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

    // ------------------------------------------------------------
    // 【JSON パース】
    // ------------------------------------------------------------
    StaticJsonDocument<512> doc;
    auto err = deserializeJson(doc, line);
    if (err) {
        sendError("unknown", "ERR_JSON_PARSE", err.c_str());
        return;
    }

    // ------------------------------------------------------------
    // 【コマンド処理】
    // ------------------------------------------------------------
    StaticJsonDocument<512> res;
    processCommand(doc.as<JsonVariantConst>(), res);
    sendJsonToSerial(res);
}
