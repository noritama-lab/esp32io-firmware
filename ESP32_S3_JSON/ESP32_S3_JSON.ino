#include <Arduino.h>
#include <ArduinoJson.h>

void setup() {
    Serial.begin(115200);
}

void loop() {
    if (!Serial.available()) return;

    // 1 行の JSON を受信
    String line = Serial.readStringUntil('\n');

    // JSON パース
    StaticJsonDocument<256> doc;
    auto err = deserializeJson(doc, line);
    if (err) {
        Serial.println("{\"status\":\"error\",\"message\":\"json parse error\"}");
        return;
    }

    const char* cmd = doc["cmd"];

    // ----------------------------------------------------------------------
    //  デジタル入力 read_dio
    //  {"cmd":"read_dio","pin":5}
    // ----------------------------------------------------------------------
    if (strcmp(cmd, "read_dio") == 0) {
        int pin = doc["pin"];
        pinMode(pin, INPUT);

        int value = digitalRead(pin);

        StaticJsonDocument<128> res;
        res["status"] = "ok";
        res["value"] = value;

        serializeJson(res, Serial);
        Serial.println();
    }

    // ----------------------------------------------------------------------
    //  デジタル出力 write_dio
    //  {"cmd":"write_dio","pin":5,"value":1}
    // ----------------------------------------------------------------------
    else if (strcmp(cmd, "write_dio") == 0) {
        int pin = doc["pin"];
        int value = doc["value"];  // 0 or 1

        pinMode(pin, OUTPUT);
        digitalWrite(pin, value ? HIGH : LOW);

        Serial.println("{\"status\":\"ok\"}");
    }

    // ----------------------------------------------------------------------
    //  ADC 読み取り read_adc
    //  {"cmd":"read_adc","pin":1}
    // ----------------------------------------------------------------------
    else if (strcmp(cmd, "read_adc") == 0) {
        int pin = doc["pin"];
        int value = analogRead(pin);

        StaticJsonDocument<128> res;
        res["status"] = "ok";
        res["value"] = value;

        serializeJson(res, Serial);
        Serial.println();
    }

    // ----------------------------------------------------------------------
    //  PWM 出力 set_pwm
    //  {"cmd":"set_pwm","pin":2,"duty":128}
    // ----------------------------------------------------------------------
    else if (strcmp(cmd, "set_pwm") == 0) {
        int pin = doc["pin"];
        int duty = doc["duty"];

        ledcAttach(pin, 5000, 8);  // 5kHz / 8bit
        ledcWrite(pin, duty);

        Serial.println("{\"status\":\"ok\"}");
    }

    // ----------------------------------------------------------------------
    //  ping
    //  {"cmd":"ping"}
    // ----------------------------------------------------------------------
    else if (strcmp(cmd, "ping") == 0) {
        Serial.println("{\"status\":\"ok\",\"message\":\"pong\"}");
    }

    // ----------------------------------------------------------------------
    //  不明コマンド
    // ----------------------------------------------------------------------
    else {
        Serial.println("{\"status\":\"error\",\"message\":\"unknown command\"}");
    }
}