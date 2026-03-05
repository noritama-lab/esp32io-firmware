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
// エラー応答（統一形式）
// ------------------------------------------------------------
void sendError(const char* code, const char* detail) {
    StaticJsonDocument<256> res;
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

    // PWM 初期化（pin ベース API）
    for (int i = 0; i < 2; i++) {
        ledcAttach(PWM_PINS[i], PWM_FREQ, PWM_RES);
        ledcWrite(PWM_PINS[i], 0);
    }
}

// ------------------------------------------------------------
// メインループ
// ------------------------------------------------------------
void loop() {
    yield();  // 安定性向上

    if (!Serial.available()) return;

    String line = Serial.readStringUntil('\n');

    StaticJsonDocument<512> doc;
    auto err = deserializeJson(doc, line);
    if (err) {
        sendError("JSON_PARSE_ERROR", "invalid json format");
        return;
    }

    const char* cmd = doc["cmd"];
    if (!cmd) {
        sendError("MISSING_CMD", "cmd field is required");
        return;
    }

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
        arr.add("ping");
        arr.add("help");

        serializeJson(res, Serial);
        Serial.println();
    }

    // ------------------------------------------------------------
    // get_status
    // ------------------------------------------------------------
    else if (strcmp(cmd, "get_status") == 0) {
        StaticJsonDocument<512> res;
        res["status"] = "ok";

        // DI
        JsonArray di = res.createNestedArray("dio_in");
        for (int i = 0; i < 6; i++) {
            di.add(digitalRead(DIO_IN_PINS[i]));
        }

        // DO
        JsonArray doo = res.createNestedArray("dio_out");
        for (int i = 0; i < 6; i++) {
            doo.add(digitalRead(DIO_OUT_PINS[i]));
        }

        // ADC
        JsonArray adc = res.createNestedArray("adc");
        for (int i = 0; i < 2; i++) {
            adc.add(readADC(ADC_PINS[i]));
        }

        // PWM（duty は保持していないため 0 を返す）
        JsonArray pwm = res.createNestedArray("pwm");
        for (int i = 0; i < 2; i++) {
            pwm.add(0);
        }

        serializeJson(res, Serial);
        Serial.println();
    }

    // ------------------------------------------------------------
    // デジタル入力 read_dio
    // {"cmd":"read_dio","pin_id":0}
    // ------------------------------------------------------------
    else if (strcmp(cmd, "read_dio") == 0) {
        int PIN_ID = doc["pin_id"];

        if (!checkRange(PIN_ID, 6)) {
            sendError("INVALID_DIO_IN_PIN_ID", "pin_id must be 0-5");
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
    // デジタル出力 write_dio
    // {"cmd":"write_dio","pin_id":3,"value":1}
    // ------------------------------------------------------------
    else if (strcmp(cmd, "write_dio") == 0) {
        int PIN_ID = doc["pin_id"];
        int value = doc["value"];

        if (!checkRange(PIN_ID, 6)) {
            sendError("INVALID_DIO_OUT_PIN_ID", "pin_id must be 0-5");
            return;
        }

        int gpio = DIO_OUT_PINS[PIN_ID];
        digitalWrite(gpio, value ? HIGH : LOW);

        Serial.println("{\"status\":\"ok\"}");
    }

    // ------------------------------------------------------------
    // ADC 読み取り read_adc
    // {"cmd":"read_adc","pin_id":1}
    // ------------------------------------------------------------
    else if (strcmp(cmd, "read_adc") == 0) {
        int PIN_ID = doc["pin_id"];

        if (!checkRange(PIN_ID, 2)) {
            sendError("INVALID_ADC_PIN_ID", "pin_id must be 0-1");
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
    // PWM 出力 set_pwm
    // {"cmd":"set_pwm","pin_id":0,"duty":128}
    // ------------------------------------------------------------
    else if (strcmp(cmd, "set_pwm") == 0) {
        int PIN_ID = doc["pin_id"];
        int duty = doc["duty"];

        if (!checkRange(PIN_ID, 2)) {
            sendError("INVALID_PWM_PIN_ID", "pin_id must be 0-1");
            return;
        }

        duty = constrain(duty, 0, 255);

        int gpio = PWM_PINS[PIN_ID];
        ledcWrite(gpio, duty);

        Serial.println("{\"status\":\"ok\"}");
    }

    // ------------------------------------------------------------
    // ping
    // ------------------------------------------------------------
    else if (strcmp(cmd, "ping") == 0) {
        Serial.println("{\"status\":\"ok\",\"message\":\"pong\"}");
    }

    // ------------------------------------------------------------
    // 不明コマンド
    // ------------------------------------------------------------
    else {
        sendError("UNKNOWN_COMMAND", "command not recognized");
    }
}