#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

// ------------------------------------------------------------
// BOOT ボタン（GPIO0）で 3 秒長押しリセット
// ------------------------------------------------------------
const int RESET_PIN = 0;  // BOOT ボタン

// ------------------------------------------------------------
// LED 状態表示（GPIO38）
// ------------------------------------------------------------
const int LED_PIN = 38;  // BUILTIN_LED（逆論理：LOW=点灯）

enum LedState {
    LED_WIFI_CONNECTING,
    LED_WIFI_OK,
    LED_AP_MODE
};

LedState ledState = LED_WIFI_CONNECTING;
unsigned long ledTimer = 0;
int blinkCount = 0;
bool ledOn = false;

// LED 更新処理（非ブロッキング）
void updateLed() {
    unsigned long now = millis();

    switch (ledState) {

        // 0.2秒周期の高速点滅
        case LED_WIFI_CONNECTING:
            if (now - ledTimer > 200) {
                ledTimer = now;
                ledOn = !ledOn;
                digitalWrite(LED_PIN, ledOn ? LOW : HIGH);
            }
            break;

        // 点灯しっぱなし
        case LED_WIFI_OK:
            digitalWrite(LED_PIN, LOW);
            break;

        // 2回点滅 → 休止 → 2回点滅…
        case LED_AP_MODE:
            if (now - ledTimer > 200) {
                ledTimer = now;

                if (blinkCount < 4) {
                    ledOn = !ledOn;
                    digitalWrite(LED_PIN, ledOn ? LOW : HIGH);
                    blinkCount++;
                } else {
                    digitalWrite(LED_PIN, HIGH); // 消灯
                    if (now - ledTimer > 800) {
                        blinkCount = 0;
                    }
                }
            }
            break;
    }
}

// ------------------------------------------------------------
// Wi-Fi 設定（初期値）
// ------------------------------------------------------------
Preferences prefs;

bool useDHCP = true;
String ipStr   = "192.168.1.50";
String maskStr = "255.255.255.0";
String gwStr   = "192.168.1.1";

String ssid = "";
String pass = "";

// ------------------------------------------------------------
// PWM 設定（初期値）
// ------------------------------------------------------------
int PWM_FREQ = 5000;
int PWM_RES  = 8;

// ------------------------------------------------------------
// 固定ピンマッピング（元コード）
// ------------------------------------------------------------
const int DIO_IN_PINS[6]  = {4, 5, 6, 7, 8, 9};
const int DIO_OUT_PINS[6] = {10, 11, 12, 13, 14, 15};
const int ADC_PINS[2]     = {1, 2};
const int PWM_PINS[2]     = {38, 39};

int PWM_DUTY[2] = {0, 0};

// ------------------------------------------------------------
// Web Server
// ------------------------------------------------------------
WebServer server(80);

// ------------------------------------------------------------
// HTML UI（軽量版）
// ------------------------------------------------------------
const char MAIN_PAGE[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>ESP32-S3 Control Panel</title>
<style>
body { font-family: sans-serif; margin: 20px; }
h2 { margin-top: 30px; }
label { display: inline-block; width: 150px; }
button { margin-top: 10px; }
</style>
</head>
<body>
<h1>ESP32-S3 Control Panel</h1>

<h2>PWM Settings</h2>
<label>Frequency:</label><input id="freq" type="number"><br>
<label>Resolution:</label><input id="res" type="number"><br>
<button onclick="setPWM()">Apply PWM</button>

<h2>Network Settings</h2>
<label>Mode:</label>
<select id="dhcp">
  <option value="1">DHCP</option>
  <option value="0">Static</option>
</select><br>
<label>IP Address:</label><input id="ip"><br>
<label>Subnet Mask:</label><input id="mask"><br>
<label>Gateway:</label><input id="gw"><br>
<button onclick="setNet()">Apply & Reboot</button>

<h2>I/O Monitor</h2>
<button onclick="refreshIO()">Refresh</button>
<pre id="io"></pre>

<script>
function loadConfig() {
  fetch('/api/get_config').then(r=>r.json()).then(cfg=>{
    document.getElementById('freq').value = cfg.pwm_freq;
    document.getElementById('res').value  = cfg.pwm_res;
    document.getElementById('dhcp').value = cfg.dhcp ? 1 : 0;
    document.getElementById('ip').value   = cfg.ip;
    document.getElementById('mask').value = cfg.mask;
    document.getElementById('gw').value   = cfg.gw;
  });
}

function setPWM() {
  let data = {
    freq: parseInt(document.getElementById('freq').value),
    res:  parseInt(document.getElementById('res').value)
  };
  fetch('/api/set_pwm_config', {method:'POST', body:JSON.stringify(data)});
}

function setNet() {
  let data = {
    dhcp: document.getElementById('dhcp').value == "1",
    ip:   document.getElementById('ip').value,
    mask: document.getElementById('mask').value,
    gw:   document.getElementById('gw').value
  };
  fetch('/api/set_network_config', {method:'POST', body:JSON.stringify(data)})
    .then(()=>{ alert("Rebooting..."); });
}

function refreshIO() {
  fetch('/api/get_io_state').then(r=>r.json()).then(j=>{
    document.getElementById('io').textContent = JSON.stringify(j, null, 2);
  });
}

loadConfig();
</script>
</body>
</html>
)HTML";

// ------------------------------------------------------------
// BOOT ボタン長押しで NVS 初期化
// ------------------------------------------------------------
void checkResetButton() {
    pinMode(RESET_PIN, INPUT_PULLUP);
    delay(50);

    if (digitalRead(RESET_PIN) == LOW) {
        unsigned long start = millis();
        while (digitalRead(RESET_PIN) == LOW) {
            if (millis() - start > 3000) {  // 3秒長押し
                Preferences prefs;
                prefs.begin("config", false);
                prefs.clear();
                prefs.end();
                delay(300);
                ESP.restart();
            }
        }
    }
}

// ------------------------------------------------------------
// PIN_ID 範囲チェック（元コード）
// ------------------------------------------------------------
bool checkRange(int id, int max) {
    return (id >= 0 && id < max);
}

// ------------------------------------------------------------
// ADC 平均化（元コード）
// ------------------------------------------------------------
int readADC(int gpio) {
    const int N = 8;
    int sum = 0;
    for (int i = 0; i < N; i++) {
        sum += analogRead(gpio);
    }
    return sum / N;
}

// ------------------------------------------------------------
// JSON エラー応答（元コード）
// ------------------------------------------------------------
void sendError(const char* cmd, const char* code, const char* detail) {
    StaticJsonDocument<192> res;
    res["status"] = "error";
    res["cmd"]    = cmd;
    res["code"]   = code;
    res["detail"] = detail;
    serializeJson(res, Serial);
    Serial.println();
}

// ------------------------------------------------------------
// JSON コマンド処理（元コード）
// ------------------------------------------------------------
void processJson(const String& line) {
    StaticJsonDocument<384> doc;
    auto err = deserializeJson(doc, line);
    if (err) {
        sendError("unknown", "ERR_JSON_PARSE", err.c_str());
        return;
    }

    if (!doc.containsKey("cmd")) {
        sendError("unknown", "ERR_MISSING_CMD", "cmd field is required");
        return;
    }

    const char* cmd = doc["cmd"];

    // 以下、元コードそのまま
    if (strcmp(cmd, "help") == 0) {
        StaticJsonDocument<256> res;
        res["status"] = "ok";
        JsonArray arr = res.createNestedArray("commands");
        arr.add("read_dio");
        arr.add("write_dio");
        arr.add("read_adc");
        arr.add("set_pwm");
        arr.add("get_status");
        arr.add("get_io_state");
        arr.add("ping");
        arr.add("help");
        serializeJson(res, Serial); Serial.println();
    }
    else if (strcmp(cmd, "get_status") == 0) {
        StaticJsonDocument<192> res;
        res["status"] = "ok";
        res["uptime_ms"] = millis();
        res["free_heap"] = esp_get_free_heap_size();
        serializeJson(res, Serial); Serial.println();
    }
    else if (strcmp(cmd, "get_io_state") == 0) {
        StaticJsonDocument<384> res;
        res["status"] = "ok";

        JsonArray di = res.createNestedArray("dio_in");
        for (int i = 0; i < 6; i++) di.add(digitalRead(DIO_IN_PINS[i]));

        JsonArray doo = res.createNestedArray("dio_out");
        for (int i = 0; i < 6; i++) doo.add(digitalRead(DIO_OUT_PINS[i]));

        JsonArray adc = res.createNestedArray("adc");
        for (int i = 0; i < 2; i++) adc.add(readADC(ADC_PINS[i]));

        JsonArray pwm = res.createNestedArray("pwm");
        for (int i = 0; i < 2; i++) pwm.add(PWM_DUTY[i]);

        serializeJson(res, Serial); Serial.println();
    }
    else if (strcmp(cmd, "read_dio") == 0) {
        int id = doc["pin_id"];
        if (!checkRange(id, 6)) {
            sendError(cmd, "ERR_INVALID_DIO_IN_PIN_ID", "pin_id must be 0-5");
            return;
        }
        int value = digitalRead(DIO_IN_PINS[id]);
        StaticJsonDocument<128> res;
        res["status"] = "ok";
        res["value"] = value;
        serializeJson(res, Serial); Serial.println();
    }
    else if (strcmp(cmd, "write_dio") == 0) {
        int id = doc["pin_id"];
        int value = doc["value"];
        if (!checkRange(id, 6)) {
            sendError(cmd, "ERR_INVALID_DIO_OUT_PIN_ID", "pin_id must be 0-5");
            return;
        }
        digitalWrite(DIO_OUT_PINS[id], value ? HIGH : LOW);
        StaticJsonDocument<96> res;
        res["status"] = "ok";
        serializeJson(res, Serial); Serial.println();
    }
    else if (strcmp(cmd, "read_adc") == 0) {
        int id = doc["pin_id"];
        if (!checkRange(id, 2)) {
            sendError(cmd, "ERR_INVALID_ADC_PIN_ID", "pin_id must be 0-1");
            return;
        }
        int value = readADC(ADC_PINS[id]);
        StaticJsonDocument<128> res;
        res["status"] = "ok";
        res["value"] = value;
        serializeJson(res, Serial); Serial.println();
    }
    else if (strcmp(cmd, "set_pwm") == 0) {
        int id = doc["pin_id"];
        int duty = doc["duty"];
        if (!checkRange(id, 2)) {
            sendError(cmd, "ERR_INVALID_PWM_PIN_ID", "pin_id must be 0-1");
            return;
        }
        duty = constrain(duty, 0, 255);
        ledcWrite(PWM_PINS[id], duty);
        PWM_DUTY[id] = duty;
        StaticJsonDocument<96> res;
        res["status"] = "ok";
        serializeJson(res, Serial); Serial.println();
    }
    else if (strcmp(cmd, "ping") == 0) {
        StaticJsonDocument<96> res;
        res["status"] = "ok";
        res["message"] = "pong";
        serializeJson(res, Serial); Serial.println();
    }
    else {
        sendError(cmd, "ERR_UNKNOWN_COMMAND", "command not recognized");
    }
}

// ------------------------------------------------------------
// API: 設定取得
// ------------------------------------------------------------
void api_get_config() {
    StaticJsonDocument<256> doc;
    doc["pwm_freq"] = PWM_FREQ;
    doc["pwm_res"]  = PWM_RES;
    doc["dhcp"]     = useDHCP;
    doc["ip"]       = ipStr;
    doc["mask"]     = maskStr;
    doc["gw"]       = gwStr;

    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
}

// ------------------------------------------------------------
// API: PWM 設定
// ------------------------------------------------------------
void api_set_pwm_config() {
    StaticJsonDocument<128> doc;
    deserializeJson(doc, server.arg("plain"));

    PWM_FREQ = doc["freq"];
    PWM_RES  = doc["res"];

    for (int i = 0; i < 2; i++) {
        ledcDetach(PWM_PINS[i]);
        ledcAttach(PWM_PINS[i], PWM_FREQ, PWM_RES);
        ledcWrite(PWM_PINS[i], PWM_DUTY[i]);
    }

    prefs.putInt("pwm_freq", PWM_FREQ);
    prefs.putInt("pwm_res",  PWM_RES);

    server.send(200, "text/plain", "OK");
}

// ------------------------------------------------------------
// API: ネットワーク設定
// ------------------------------------------------------------
void api_set_network_config() {
    StaticJsonDocument<256> doc;
    deserializeJson(doc, server.arg("plain"));

    useDHCP = doc["dhcp"];
    ipStr   = (const char*)doc["ip"];
    maskStr = (const char*)doc["mask"];
    gwStr   = (const char*)doc["gw"];

    prefs.putBool("dhcp", useDHCP);
    prefs.putString("ip",   ipStr);
    prefs.putString("mask", maskStr);
    prefs.putString("gw",   gwStr);

    server.send(200, "text/plain", "REBOOT");

    delay(500);
    ESP.restart();
}

// ------------------------------------------------------------
// API: I/O 状態（Web UI 用）
// ------------------------------------------------------------
void api_get_io_state() {
    StaticJsonDocument<384> res;
    res["status"] = "ok";

    JsonArray di = res.createNestedArray("dio_in");
    for (int i = 0; i < 6; i++) di.add(digitalRead(DIO_IN_PINS[i]));

    JsonArray doo = res.createNestedArray("dio_out");
    for (int i = 0; i < 6; i++) doo.add(digitalRead(DIO_OUT_PINS[i]));

    JsonArray adc = res.createNestedArray("adc");
    for (int i = 0; i < 2; i++) adc.add(readADC(ADC_PINS[i]));

    JsonArray pwm = res.createNestedArray("pwm");
    for (int i = 0; i < 2; i++) pwm.add(PWM_DUTY[i]);

    String out;
    serializeJson(res, out);
    server.send(200, "application/json", out);
}

// ------------------------------------------------------------
// Wi-Fi 接続（失敗したら AP モード）
// ------------------------------------------------------------
bool connectWiFi() {
    WiFi.mode(WIFI_STA);

    if (useDHCP) {
        WiFi.begin(ssid.c_str(), pass.c_str());
    } else {
        IPAddress ip, gw, mask;
        ip.fromString(ipStr);
        gw.fromString(gwStr);
        mask.fromString(maskStr);
        WiFi.config(ip, gw, mask);
        WiFi.begin(ssid.c_str(), pass.c_str());
    }

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > 8000) return false;
        delay(200);
    }
    return true;
}

void startAPFallback() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP("ESP32-Setup", "12345678");
    ledState = LED_AP_MODE;  // LED 表示
}

// ------------------------------------------------------------
// setup
// ------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    delay(100);

    // LED 初期化
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH); // 消灯
    ledState = LED_WIFI_CONNECTING;

    checkResetButton();

    prefs.begin("config", false);

    PWM_FREQ = prefs.getInt("pwm_freq", PWM_FREQ);
    PWM_RES  = prefs.getInt("pwm_res",  PWM_RES);

    useDHCP = prefs.getBool("dhcp", true);
    ipStr   = prefs.getString("ip",   ipStr);
    maskStr = prefs.getString("mask", maskStr);
    gwStr   = prefs.getString("gw",   gwStr);

    for (int i = 0; i < 6; i++) {
        pinMode(DIO_IN_PINS[i], INPUT);
        pinMode(DIO_OUT_PINS[i], OUTPUT);
        digitalWrite(DIO_OUT_PINS[i], LOW);
    }

    for (int i = 0; i < 2; i++) {
        ledcAttach(PWM_PINS[i], PWM_FREQ, PWM_RES);
        ledcWrite(PWM_PINS[i], 0);
        PWM_DUTY[i] = 0;
    }

    // Wi-Fi 接続
    if (connectWiFi()) {
        ledState = LED_WIFI_OK;  // ★ 接続成功 → LED 点灯
    } else {
        startAPFallback();       // ★ AP モード → 2回点滅
    }

    // Web API
    server.on("/", [](){ server.send_P(200, "text/html", MAIN_PAGE); });
    server.on("/api/get_config", api_get_config);
    server.on("/api/set_pwm_config", HTTP_POST, api_set_pwm_config);
    server.on("/api/set_network_config", HTTP_POST, api_set_network_config);
    server.on("/api/get_io_state", api_get_io_state);

    server.begin();
}

// ------------------------------------------------------------
// loop
// ------------------------------------------------------------
void loop() {
    server.handleClient();

    // LED 状態更新（非ブロッキング）
    updateLed();

    // JSON コマンド処理（元コード）
    static String buf;
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n') {
            processJson(buf);
            buf = "";
        } else {
            buf += c;
        }
    }
}
