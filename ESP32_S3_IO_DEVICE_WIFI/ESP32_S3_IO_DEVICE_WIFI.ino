#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <WebServer.h>

// ------------------------------------------------------------
// 固定ピンマッピング
// ------------------------------------------------------------
const int DIO_IN_PINS[6]  = {4, 5, 6, 7, 8, 9};
const int DIO_OUT_PINS[6] = {10, 11, 12, 13, 14, 15};
const int ADC_PINS[2]     = {1, 2};
const int PWM_PINS[2]     = {38, 39};

// --- RGB LED (WS2812) ---
const int RGB_PIN = 48;
const int PIXEL_COUNT = 1;
const uint8_t DEFAULT_RGB_BRIGHTNESS = 64;

// デフォルト PWM 設定
const int DEFAULT_PWM_FREQ = 5000;
const int DEFAULT_PWM_RES  = 8;

int PWM_FREQ = DEFAULT_PWM_FREQ;
int PWM_RES  = DEFAULT_PWM_RES;
int PWM_DUTY[2] = {0, 0};

Preferences prefs;
Adafruit_NeoPixel rgb(PIXEL_COUNT, RGB_PIN, NEO_GRB + NEO_KHZ800);

// HTTP サーバ
WebServer server(80);

uint8_t currentR = 0;
uint8_t currentG = 0;
uint8_t currentB = 0;
uint8_t currentBrightness = DEFAULT_RGB_BRIGHTNESS;
bool ledOn = false;

// ------------------------------------------------------------
// PWM 関連
// ------------------------------------------------------------
int pwmMaxDuty() {
    return (1UL << PWM_RES) - 1;
}

int pwmSafeMaxDuty() {
    int maxDuty = pwmMaxDuty();
    if (PWM_RES >= 14 && maxDuty > 0) return maxDuty - 1;
    return maxDuty;
}

int scaleDuty(int duty, int oldMax, int newMax) {
    if (oldMax <= 0) return 0;
    return (static_cast<long>(duty) * newMax) / oldMax;
}

bool applyPWMConfig(bool resetDuty = false) {
    int maxDuty = pwmSafeMaxDuty();
    bool okAll = true;

    for (int i = 0; i < 2; i++) {
        ledcDetach(PWM_PINS[i]);
        bool ok = ledcAttach(PWM_PINS[i], PWM_FREQ, PWM_RES);
        if (!ok) okAll = false;

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
    return (id >= 0 && id < max);
}

// ------------------------------------------------------------
// ADC 平均化
// ------------------------------------------------------------
int readADC(int gpio) {
    const int N = 8;
    int sum = 0;
    for (int i = 0; i < N; i++) sum += analogRead(gpio);
    return sum / N;
}

// ------------------------------------------------------------
// LED 反映
// ------------------------------------------------------------
void applyLed() {
    rgb.setBrightness(currentBrightness);
    uint8_t r = ledOn ? currentR : 0;
    uint8_t g = ledOn ? currentG : 0;
    uint8_t b = ledOn ? currentB : 0;
    rgb.setPixelColor(0, rgb.Color(r, g, b));
    rgb.show();
}

// ------------------------------------------------------------
// エラー応答
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
// 非ブロッキング行読み取り
// ------------------------------------------------------------
bool readLine(String& out, bool& overflowed) {
    static String buf;
    const size_t MAX_LINE_LEN = 512;
    overflowed = false;

    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n') {
            out = buf;
            buf = "";
            return true;
        }
        if (buf.length() < MAX_LINE_LEN) buf += c;
        else overflowed = true;
    }
    return false;
}

// ------------------------------------------------------------
// PWM 設定の保存・読み込み
// ------------------------------------------------------------
int loadPWMFreq() {
    prefs.begin("esp32io", true);
    int v = prefs.getInt("pwm_freq", DEFAULT_PWM_FREQ);
    prefs.end();
    return v;
}

int loadPWMRes() {
    prefs.begin("esp32io", true);
    int v = prefs.getInt("pwm_res", DEFAULT_PWM_RES);
    prefs.end();
    return v;
}

void savePWMFreq(int v) {
    prefs.begin("esp32io", false);
    prefs.putInt("pwm_freq", v);
    prefs.end();
}

void savePWMRes(int v) {
    prefs.begin("esp32io", false);
    prefs.putInt("pwm_res", v);
    prefs.end();
}

// ------------------------------------------------------------
// コマンド処理（Serial / HTTP 共通）
// ------------------------------------------------------------
void processCommand(JsonVariantConst doc, JsonDocument& res) {
    const char* cmd = doc["cmd"] | "unknown";

    // help
    if (strcmp(cmd, "help") == 0) {
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
        arr.add("set_brightness");
        arr.add("led_on");
        arr.add("led_off");
        arr.add("get_led_state");
        arr.add("ping");
        arr.add("help");
        return;
    }

    // get_status
    if (strcmp(cmd, "get_status") == 0) {
        res["uptime_ms"] = millis();
        res["status"] = "ok";
        res["free_heap"] = esp_get_free_heap_size();
        return;
    }

    // get_io_state
    if (strcmp(cmd, "get_io_state") == 0) {
        JsonArray di = res.createNestedArray("dio_in");
        res["status"] = "ok";
        for (int i = 0; i < 6; i++) di.add(digitalRead(DIO_IN_PINS[i]));

        JsonArray doo = res.createNestedArray("dio_out");
        for (int i = 0; i < 6; i++) doo.add(digitalRead(DIO_OUT_PINS[i]));

        JsonArray adc = res.createNestedArray("adc");
        for (int i = 0; i < 2; i++) adc.add(readADC(ADC_PINS[i]));

        JsonArray pwm = res.createNestedArray("pwm");
        for (int i = 0; i < 2; i++) pwm.add(PWM_DUTY[i]);
        return;
    }

    // read_di
    if (strcmp(cmd, "read_di") == 0) {
        int id = doc["pin_id"];
        if (!checkRange(id, 6)) return;
        res["status"] = "ok";
        res["value"] = digitalRead(DIO_IN_PINS[id]);
        return;
    }

    // set_do
    if (strcmp(cmd, "set_do") == 0) {
        int id = doc["pin_id"];
        int value = doc["value"];
        if (!checkRange(id, 6)) return;
        res["status"] = "ok";
        digitalWrite(DIO_OUT_PINS[id], value ? HIGH : LOW);
        return;
    }

    // read_adc
    if (strcmp(cmd, "read_adc") == 0) {
        int id = doc["pin_id"];
        if (!checkRange(id, 2)) return;
        res["status"] = "ok";
        res["value"] = readADC(ADC_PINS[id]);
        return;
    }

    // set_pwm
    if (strcmp(cmd, "set_pwm") == 0) {
        int id = doc["pin_id"];
        int duty = doc["duty"];
        if (!checkRange(id, 2)) return;
        duty = constrain(duty, 0, pwmSafeMaxDuty());
        PWM_DUTY[id] = duty;
        ledcWrite(PWM_PINS[id], duty);
        res["status"] = "ok";
        res["duty"] = duty;
        res["max_duty"] = pwmSafeMaxDuty();
        return;
    }

    // get_pwm_config
    if (strcmp(cmd, "get_pwm_config") == 0) {
        res["freq"] = PWM_FREQ;
        res["res"]  = PWM_RES;
        return;
    }

    // set_pwm_config
    if (strcmp(cmd, "set_pwm_config") == 0) {
        int freq = doc["freq"];
        int r    = doc["res"];
        if (freq < 1 || freq > 20000) return;
        if (r < 1 || r > 14) return;

        int oldFreq = PWM_FREQ;
        int oldRes  = PWM_RES;
        int oldMax  = pwmMaxDuty();
        int oldDuty[2] = {PWM_DUTY[0], PWM_DUTY[1]};

        PWM_FREQ = freq;
        PWM_RES  = r;

        int newMax = pwmMaxDuty();
        for (int i = 0; i < 2; i++)
            PWM_DUTY[i] = scaleDuty(PWM_DUTY[i], oldMax, newMax);

        if (!applyPWMConfig(false)) {
            PWM_FREQ = oldFreq;
            PWM_RES  = oldRes;
            for (int i = 0; i < 2; i++) PWM_DUTY[i] = oldDuty[i];
            applyPWMConfig(false);
            return;
        }

        savePWMFreq(freq);
        savePWMRes(r);
        res["status"] = "ok";
        res["freq"] = PWM_FREQ;
        res["res"]  = PWM_RES;
        res["max_duty"] = pwmSafeMaxDuty();
        return;
    }

    // set_rgb
    if (strcmp(cmd, "set_rgb") == 0) {
        currentR = doc["r"];
        currentG = doc["g"];
        currentB = doc["b"];
        if (doc.containsKey("brightness"))
            currentBrightness = doc["brightness"];
        ledOn = true;
        res["status"] = "ok";
        applyLed();
        return;
    }

    // set_brightness
    if (strcmp(cmd, "set_brightness") == 0) {
        currentBrightness = doc["brightness"];
        res["status"] = "ok";
        applyLed();
        return;
    }

    // led_on
    if (strcmp(cmd, "led_on") == 0) {
        ledOn = true;
        res["status"] = "ok";
        applyLed();
        return;
    }

    // led_off
    if (strcmp(cmd, "led_off") == 0) {
        ledOn = false;
        res["status"] = "ok";
        applyLed();
        return;
    }

    // get_led_state
    if (strcmp(cmd, "get_led_state") == 0) {
        res["pin"] = RGB_PIN;
        res["on"]  = ledOn;
        res["r"]   = currentR;
        res["g"]   = currentG;
        res["b"]   = currentB;
        res["brightness"] = currentBrightness;
        res["status"] = "ok";
        return;
    }

    // ping
    if (strcmp(cmd, "ping") == 0) {
        res["message"] = "pong";
        res["status"] = "ok";
        return;
    }

    res["status"] = "error";
    res["message"] = "unknown command";
}

// ------------------------------------------------------------
// HTTP サーバ
// ------------------------------------------------------------
void setupWebServer() {
    server.on("/api", HTTP_POST, []() {
        StaticJsonDocument<512> doc;
        StaticJsonDocument<512> res;

        if (!server.hasArg("plain")) {
            server.send(400, "application/json", "{\"error\":\"no body\"}");
            return;
        }

        auto err = deserializeJson(doc, server.arg("plain"));
        if (err) {
            server.send(400, "application/json", "{\"error\":\"json parse\"}");
            return;
        }

        processCommand(doc.as<JsonVariantConst>(), res);

        String out;
        serializeJson(res, out);
        server.send(200, "application/json", out);
    });

    server.begin();
}

// ------------------------------------------------------------
// AP モード起動
// ------------------------------------------------------------
void startApMode() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP("ESP32_S3_IO", "12345678");
    Serial.println("AP Started: " + WiFi.softAPIP().toString());
}

// ------------------------------------------------------------
// 初期化
// ------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    delay(100);

    PWM_FREQ = loadPWMFreq();
    PWM_RES  = loadPWMRes();

    for (int i = 0; i < 6; i++) {
        pinMode(DIO_IN_PINS[i], INPUT);
        pinMode(DIO_OUT_PINS[i], OUTPUT);
        digitalWrite(DIO_OUT_PINS[i], LOW);
    }

    applyPWMConfig(true);

    rgb.begin();
    applyLed();

    startApMode();
    setupWebServer();
}

// ------------------------------------------------------------
// メインループ
// ------------------------------------------------------------
void loop() {
    server.handleClient();
    yield(); // WiFiスタックに処理時間を譲る

    String line;
    bool overflowed = false;
    if (readLine(line, overflowed)) {
        line.trim();
        if (line.length() > 0) {
            StaticJsonDocument<512> doc;
            StaticJsonDocument<512> res;

            auto err = deserializeJson(doc, line);
            if (!err) {
                processCommand(doc.as<JsonVariantConst>(), res);
                serializeJson(res, Serial);
                Serial.println();
            }
        }
    }

    delay(1); // 省電力と安定性のための微小待ち
}
