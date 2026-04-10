#include <Arduino.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>

// ESP32-S3 の内蔵RGB LED(WS2812系)を想定
const int RGB_PIN = 48;
const int PIXEL_COUNT = 1;
const uint8_t DEFAULT_BRIGHTNESS = 64;

Adafruit_NeoPixel rgb(PIXEL_COUNT, RGB_PIN, NEO_GRB + NEO_KHZ800);

uint8_t currentR = 0;
uint8_t currentG = 0;
uint8_t currentB = 0;
uint8_t currentBrightness = DEFAULT_BRIGHTNESS;
bool ledOn = false;

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

void applyLed() {
    rgb.setBrightness(currentBrightness);

    uint8_t r = ledOn ? currentR : 0;
    uint8_t g = ledOn ? currentG : 0;
    uint8_t b = ledOn ? currentB : 0;

    rgb.setPixelColor(0, rgb.Color(r, g, b));
    rgb.show();
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
// 初期化
// ------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    delay(100);

    rgb.begin();
    rgb.setBrightness(DEFAULT_BRIGHTNESS);
    applyLed();
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

    StaticJsonDocument<256> doc;
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

    if (strcmp(cmd, "help") == 0) {
        StaticJsonDocument<192> res;
        res["status"] = "ok";
        JsonArray arr = res.createNestedArray("commands");
        arr.add("set_rgb");
        arr.add("set_brightness");
        arr.add("led_on");
        arr.add("led_off");
        arr.add("get_led_state");
        arr.add("ping");
        arr.add("help");
        serializeJson(res, Serial);
        Serial.println();
    }

    else if (strcmp(cmd, "set_rgb") == 0) {
        if (!doc.containsKey("r") || !doc.containsKey("g") || !doc.containsKey("b")) {
            sendError(cmd, "ERR_MISSING_PARAM", "r, g, b are required");
            return;
        }

        int r = doc["r"];
        int g = doc["g"];
        int b = doc["b"];

        if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255) {
            sendError(cmd, "ERR_INVALID_RGB", "r, g, b must be 0-255");
            return;
        }

        currentR = static_cast<uint8_t>(r);
        currentG = static_cast<uint8_t>(g);
        currentB = static_cast<uint8_t>(b);

        if (doc.containsKey("brightness")) {
            int brightness = doc["brightness"];
            if (brightness < 0 || brightness > 255) {
                sendError(cmd, "ERR_INVALID_BRIGHTNESS", "brightness must be 0-255");
                return;
            }
            currentBrightness = static_cast<uint8_t>(brightness);
        }

        ledOn = true;
        applyLed();

        StaticJsonDocument<128> res;
        res["status"] = "ok";
        res["on"] = ledOn;
        res["r"] = currentR;
        res["g"] = currentG;
        res["b"] = currentB;
        res["brightness"] = currentBrightness;
        serializeJson(res, Serial);
        Serial.println();
    }

    else if (strcmp(cmd, "set_brightness") == 0) {
        if (!doc.containsKey("brightness")) {
            sendError(cmd, "ERR_MISSING_PARAM", "brightness is required");
            return;
        }

        int brightness = doc["brightness"];
        if (brightness < 0 || brightness > 255) {
            sendError(cmd, "ERR_INVALID_BRIGHTNESS", "brightness must be 0-255");
            return;
        }

        currentBrightness = static_cast<uint8_t>(brightness);
        applyLed();

        StaticJsonDocument<96> res;
        res["status"] = "ok";
        res["brightness"] = currentBrightness;
        serializeJson(res, Serial);
        Serial.println();
    }

    else if (strcmp(cmd, "led_on") == 0) {
        ledOn = true;
        applyLed();

        StaticJsonDocument<96> res;
        res["status"] = "ok";
        res["on"] = ledOn;
        serializeJson(res, Serial);
        Serial.println();
    }

    else if (strcmp(cmd, "led_off") == 0) {
        ledOn = false;
        applyLed();

        StaticJsonDocument<96> res;
        res["status"] = "ok";
        res["on"] = ledOn;
        serializeJson(res, Serial);
        Serial.println();
    }

    else if (strcmp(cmd, "get_led_state") == 0) {
        StaticJsonDocument<128> res;
        res["status"] = "ok";
        res["pin"] = RGB_PIN;
        res["on"] = ledOn;
        res["r"] = currentR;
        res["g"] = currentG;
        res["b"] = currentB;
        res["brightness"] = currentBrightness;
        serializeJson(res, Serial);
        Serial.println();
    }

    else if (strcmp(cmd, "ping") == 0) {
        StaticJsonDocument<96> res;
        res["status"] = "ok";
        res["message"] = "pong";
        serializeJson(res, Serial);
        Serial.println();
    }

    else {
        sendError(cmd, "ERR_UNKNOWN_COMMAND", "command not recognized");
    }
}