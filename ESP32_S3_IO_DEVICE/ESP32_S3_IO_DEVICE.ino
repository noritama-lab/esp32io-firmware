#include <Arduino.h>
#include <ArduinoJson.h>

// ------------------------------------------------------------
// 固定ピンマッピング（PIN_ID → 実 GPIO）
// ------------------------------------------------------------

// デジタル入力 6 点
const int DIO_IN_PINS[6] = {4, 5, 6, 7, 8, 9};

// デジタル出力 6 点
const int DIO_OUT_PINS[6] = {10, 11, 12, 13, 14, 15};

// アナログ入力 2 点
const int ADC_PINS[2] = {1, 2};

// PWM 出力 2 点
const int PWM_PINS[2] = {38, 39};

// PWM 設定
const int PWM_FREQ = 5000;  // 5kHz
const int PWM_RES  = 8;     // 8bit

// PWM duty の保持（get_io_state 用）
int PWM_DUTY[2] = {0, 0};

// ------------------------------------------------------------
// PIN_ID が範囲内かチェック
// ------------------------------------------------------------
bool checkRange(int PIN_ID, int max) {
    return (PIN_ID >= 0 && PIN_ID < max);
}

// ------------------------------------------------------------
// ADC 平均化（ノイズ対策）
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
// エラー応答（エラーコードは ERR_XXXX で統一）
// ------------------------------------------------------------
void sendError(const char* code, const char* detail) {
    StaticJsonDocument<256> res;  // 256 bytes: error JSON に十分
    res["status"] = "error";
    res["code"] = code;
    res["detail"] = detail;
    serializeJson(res, Serial);
    Serial.println();
}

// ------------------------------------------------------------
// 初期化
// ------------------------------------------------------------
void setup() {
    Serial.begin(115200);

    // デジタル入力
    for (int i = 0; i < 6; i++) {
        pinMode(DIO_IN_PINS[i], INPUT);
    }

    // デジタル出力
    for (int i = 0; i < 6; i++) {
        pinMode(DIO_OUT_PINS[i], OUTPUT);
        digitalWrite(DIO_OUT_PINS[i], LOW);
    }

    // PWM 初期化
    for (int i = 0; i < 2; i++) {
        ledcAttach(PWM_PINS[i], PWM_FREQ, PWM_RES);
        ledcWrite(PWM_PINS[i], 0);
        PWM_DUTY[i] = 0;
    }
}

// ------------------------------------------------------------
// メインループ
// ------------------------------------------------------------
void loop() {
    yield();

    if (!Serial.available()) return;

    String line = Serial.readStringUntil('\n');
    line.trim();  // 改行・空白除去で堅牢性アップ

    StaticJsonDocument<512> doc;  // 512 bytes: 最大コマンド JSON に十分
    auto err = deserializeJson(doc, line);
    if (err) {
        sendError("ERR_JSON_PARSE", err.c_str());
        return;
    }

    // cmd が存在し、文字列であることを保証
    if (!doc.containsKey("cmd")) {
        sendError("ERR_MISSING_CMD", "cmd field is required");
        return;
    }
    if (!doc["cmd"].is<const char*>()) {
        sendError("ERR_INVALID_CMD_TYPE", "cmd must be a string");
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
        arr.add("read_dio");
        arr.add("write_dio");
        arr.add("read_adc");
        arr.add("set_pwm");
        arr.add("get_status");
        arr.add("get_io_state");
        arr.add("ping");
        arr.add("help");

        serializeJson(res, Serial);
        Serial.println();
    }

    // ------------------------------------------------------------
    // get_status
    // ------------------------------------------------------------
    else if (strcmp(cmd, "get_status") == 0) {
        StaticJsonDocument<256> res;
        res["status"] = "ok";
        res["uptime_ms"] = millis();
        res["free_heap"] = esp_get_free_heap_size();

        serializeJson(res, Serial);
        Serial.println();
    }

    // ------------------------------------------------------------
    // get_io_state
    // ------------------------------------------------------------
    else if (strcmp(cmd, "get_io_state") == 0) {
        StaticJsonDocument<512> res;
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
    // read_dio
    // ------------------------------------------------------------
    else if (strcmp(cmd, "read_dio") == 0) {
        int PIN_ID = doc["pin_id"];

        if (!checkRange(PIN_ID, 6)) {
            sendError("ERR_INVALID_DIO_IN_PIN_ID", "pin_id must be 0-5");
            return;
        }

        int gpio = DIO_IN_PINS[PIN_ID];
        int value = digitalRead(gpio);

        StaticJsonDocument<128> res;
        res["status"] = "ok";
        res["value"] = value;
        serializeJson(res, Serial);
        Serial.println();
    }

    // ------------------------------------------------------------
    // write_dio
    // ------------------------------------------------------------
    else if (strcmp(cmd, "write_dio") == 0) {
        int PIN_ID = doc["pin_id"];
        int value = doc["value"];

        if (!checkRange(PIN_ID, 6)) {
            sendError("ERR_INVALID_DIO_OUT_PIN_ID", "pin_id must be 0-5");
            return;
        }

        if (!(value == 0 || value == 1)) {
            sendError("ERR_INVALID_VALUE", "value must be 0 or 1");
            return;
        }

        int gpio = DIO_OUT_PINS[PIN_ID];
        digitalWrite(gpio, value ? HIGH : LOW);

        StaticJsonDocument<128> res;
        res["status"] = "ok";
        serializeJson(res, Serial);
        Serial.println();
    }

    // ------------------------------------------------------------
    // read_adc
    // ------------------------------------------------------------
    else if (strcmp(cmd, "read_adc") == 0) {
        int PIN_ID = doc["pin_id"];

        if (!checkRange(PIN_ID, 2)) {
            sendError("ERR_INVALID_ADC_PIN_ID", "pin_id must be 0-1");
            return;
        }

        int gpio = ADC_PINS[PIN_ID];
        int value = readADC(gpio);

        StaticJsonDocument<128> res;
        res["status"] = "ok";
        res["value"] = value;
        serializeJson(res, Serial);
        Serial.println();
    }

    // ------------------------------------------------------------
    // set_pwm
    // ------------------------------------------------------------
    else if (strcmp(cmd, "set_pwm") == 0) {
        int PIN_ID = doc["pin_id"];
        int duty = doc["duty"];

        if (!checkRange(PIN_ID, 2)) {
            sendError("ERR_INVALID_PWM_PIN_ID", "pin_id must be 0-1");
            return;
        }

        duty = constrain(duty, 0, 255);

        int gpio = PWM_PINS[PIN_ID];
        ledcWrite(gpio, duty);
        PWM_DUTY[PIN_ID] = duty;

        StaticJsonDocument<128> res;
        res["status"] = "ok";
        serializeJson(res, Serial);
        Serial.println();
    }

    // ------------------------------------------------------------
    // ping
    // ------------------------------------------------------------
    else if (strcmp(cmd, "ping") == 0) {
        StaticJsonDocument<128> res;
        res["status"] = "ok";
        res["message"] = "pong";
        serializeJson(res, Serial);
        Serial.println();
    }

    // ------------------------------------------------------------
    // 不明コマンド
    // ------------------------------------------------------------
    else {
        sendError("ERR_UNKNOWN_COMMAND", "command not recognized");
    }
}