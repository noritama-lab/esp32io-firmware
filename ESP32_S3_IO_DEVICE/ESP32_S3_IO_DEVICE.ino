#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>

// ------------------------------------------------------------
// 固定ピンマッピング
// ------------------------------------------------------------
const int DIO_IN_PINS[6]  = {4, 5, 6, 7, 8, 9};
const int DIO_OUT_PINS[6] = {10, 11, 12, 13, 14, 15};
const int ADC_PINS[2]     = {1, 2};
const int PWM_PINS[2]     = {38, 39};

// デフォルト PWM 設定
const int DEFAULT_PWM_FREQ = 5000;
const int DEFAULT_PWM_RES  = 8;

int PWM_FREQ = DEFAULT_PWM_FREQ;
int PWM_RES  = DEFAULT_PWM_RES;
int PWM_DUTY[2] = {0, 0};

Preferences prefs;

int pwmMaxDuty() {
    return (1UL << PWM_RES) - 1;
}

int pwmSafeMaxDuty() {
    int maxDuty = pwmMaxDuty();
    if (PWM_RES >= 14 && maxDuty > 0) {
        return maxDuty - 1;
    }
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
    return (id >= 0 && id < max);
}

// ------------------------------------------------------------
// ADC 平均化
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
bool readLine(String& out) {
    static String buf;

    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n') {
            out = buf;
            buf = "";
            return true;
        }
        buf += c;
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
}

// ------------------------------------------------------------
// メインループ
// ------------------------------------------------------------
void loop() {
    yield();

    String line;
    if (!readLine(line)) return;
    line.trim();
    if (line.length() == 0) return;

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
    if (!doc["cmd"].is<const char*>()) {
        sendError("unknown", "ERR_INVALID_CMD_TYPE", "cmd must be a string");
        return;
    }

    const char* cmd = doc["cmd"];

    // ------------------------------------------------------------
    // help
    // ------------------------------------------------------------
    if (strcmp(cmd, "help") == 0) {
        StaticJsonDocument<256> res;
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
        serializeJson(res, Serial);
        Serial.println();
    }

    // ------------------------------------------------------------
    else if (strcmp(cmd, "get_status") == 0) {
        StaticJsonDocument<192> res;
        res["status"] = "ok";
        res["uptime_ms"] = millis();
        res["free_heap"] = esp_get_free_heap_size();
        serializeJson(res, Serial);
        Serial.println();
    }

    // ------------------------------------------------------------
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

        serializeJson(res, Serial);
        Serial.println();
    }

    // ------------------------------------------------------------
    else if (strcmp(cmd, "read_di") == 0) {
        int id = doc["pin_id"];
        if (!checkRange(id, 6)) {
            sendError(cmd, "ERR_INVALID_DIO_IN_PIN_ID", "pin_id must be 0-5");
            return;
        }

        int value = digitalRead(DIO_IN_PINS[id]);

        StaticJsonDocument<128> res;
        res["status"] = "ok";
        res["value"] = value;
        serializeJson(res, Serial);
        Serial.println();
    }

    // ------------------------------------------------------------
    else if (strcmp(cmd, "set_do") == 0) {
        int id = doc["pin_id"];
        int value = doc["value"];

        if (!checkRange(id, 6)) {
            sendError(cmd, "ERR_INVALID_DIO_OUT_PIN_ID", "pin_id must be 0-5");
            return;
        }
        if (!(value == 0 || value == 1)) {
            sendError(cmd, "ERR_INVALID_VALUE", "value must be 0 or 1");
            return;
        }

        digitalWrite(DIO_OUT_PINS[id], value ? HIGH : LOW);

        StaticJsonDocument<96> res;
        res["status"] = "ok";
        serializeJson(res, Serial);
        Serial.println();
    }

    // ------------------------------------------------------------
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
        serializeJson(res, Serial);
        Serial.println();
    }

    // ------------------------------------------------------------
    else if (strcmp(cmd, "set_pwm") == 0) {
        int id = doc["pin_id"];
        int duty = doc["duty"];

        if (!checkRange(id, 2)) {
            sendError(cmd, "ERR_INVALID_PWM_PIN_ID", "pin_id must be 0-1");
            return;
        }

        duty = constrain(duty, 0, pwmSafeMaxDuty());
        ledcWrite(PWM_PINS[id], duty);
        PWM_DUTY[id] = duty;

        StaticJsonDocument<128> res;
        res["status"] = "ok";
        res["duty"] = duty;
        res["max_duty"] = pwmSafeMaxDuty();
        serializeJson(res, Serial);
        Serial.println();
    }

    // ------------------------------------------------------------
    else if (strcmp(cmd, "get_pwm_config") == 0) {
        StaticJsonDocument<128> res;
        res["status"] = "ok";
        res["freq"] = PWM_FREQ;
        res["res"]  = PWM_RES;
        serializeJson(res, Serial);
        Serial.println();
    }

    // ------------------------------------------------------------
    else if (strcmp(cmd, "set_pwm_config") == 0) {
        if (!doc.containsKey("freq") || !doc.containsKey("res")) {
            sendError(cmd, "ERR_MISSING_PARAM", "freq and res are required");
            return;
        }

        int freq = doc["freq"];
        int res  = doc["res"];

        if (freq < 1 || freq > 20000) {
            sendError(cmd, "ERR_INVALID_FREQ", "freq must be 1-20000");
            return;
        }

        if (res < 1 || res > 14) {
            sendError(cmd, "ERR_INVALID_RES", "res must be 1-14");
            return;
        }

        int oldFreq = PWM_FREQ;
        int oldRes  = PWM_RES;
        int oldMax = pwmMaxDuty();
        int oldDuty[2] = {PWM_DUTY[0], PWM_DUTY[1]};

        PWM_FREQ = freq;
        PWM_RES  = res;

        int newMax = pwmMaxDuty();
        for (int i = 0; i < 2; i++) {
            PWM_DUTY[i] = scaleDuty(PWM_DUTY[i], oldMax, newMax);
        }

        if (!applyPWMConfig(false)) {
            PWM_FREQ = oldFreq;
            PWM_RES  = oldRes;
            for (int i = 0; i < 2; i++) {
                PWM_DUTY[i] = oldDuty[i];
            }
            applyPWMConfig(false);
            sendError(cmd, "ERR_PWM_ATTACH", "failed to apply PWM frequency/resolution");
            return;
        }

        savePWMFreq(freq);
        savePWMRes(res);

        StaticJsonDocument<160> resdoc;
        resdoc["status"] = "ok";
        resdoc["freq"] = freq;
        resdoc["res"]  = res;
        resdoc["max_duty"] = pwmSafeMaxDuty();
        serializeJson(resdoc, Serial);
        Serial.println();
    }

    // ------------------------------------------------------------
    else if (strcmp(cmd, "ping") == 0) {
        StaticJsonDocument<96> res;
        res["status"] = "ok";
        res["message"] = "pong";
        serializeJson(res, Serial);
        Serial.println();
    }

    // ------------------------------------------------------------
    else {
        sendError(cmd, "ERR_UNKNOWN_COMMAND", "command not recognized");
    }
}