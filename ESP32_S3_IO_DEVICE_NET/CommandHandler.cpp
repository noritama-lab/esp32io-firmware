/**
 * @file CommandHandler.cpp
 * @brief JSON command processing engine.
 * @copyright Copyright (c) 2024 norit. Licensed under the MIT License.
 */
#include "CommandHandler.h"
#include "NetworkManager.h"

/**
 * @brief Entry point for processing commands from Serial or HTTP.
 * Dispatches requests to HardwareManager and formats JSON responses.
 */
void CommandHandler::process(JsonVariantConst req, JsonDocument& res) {
    const char* cmd = req["cmd"] | "";
    
    // --- DIO / ADC / PWM Controls ---
    if (strcmp(cmd, "read_di") == 0) {
        int id = req["pin_id"] | -1;
        if (!checkRange(id, DIO_IN_COUNT, res, cmd)) return;
        res["status"] = "ok";
        res["value"] = Hardware.readDI(id);
    }
    else if (strcmp(cmd, "set_do") == 0) {
        int id = req["pin_id"] | -1;
        int val = req["value"] | 0;
        if (!checkRange(id, DIO_OUT_COUNT, res, cmd)) return;
        Hardware.writeDO(id, val);
        res["status"] = "ok";
    } 
    /**
     * @brief Aggregates all IO states into a single JSON object.
     * Useful for dashboard synchronization.
     */
    else if (strcmp(cmd, "get_io_state") == 0) {
        res["status"] = "ok";
        JsonArray di = res["dio_in"].to<JsonArray>();
        for (int i = 0; i < DIO_IN_COUNT; i++) di.add(Hardware.readDI(i));
        
        JsonArray doo = res["dio_out"].to<JsonArray>();
        for (int i = 0; i < DIO_OUT_COUNT; i++) doo.add(digitalRead(DIO_OUT_PINS[i]));

        JsonArray adc = res["adc"].to<JsonArray>();
        for (int i = 0; i < ADC_COUNT; i++) adc.add(Hardware.readADCValue(i));

        JsonArray pwm = res["pwm"].to<JsonArray>();
        for (int i = 0; i < PWM_COUNT; i++) pwm.add(Hardware.getDuty(i));
    }
    else if (strcmp(cmd, "read_adc") == 0) {
        int id = req["pin_id"] | -1;
        if (!checkRange(id, ADC_COUNT, res, cmd)) return;
        res["status"] = "ok";
        res["value"] = Hardware.readADCValue(id);
    }
    else if (strcmp(cmd, "set_pwm") == 0) {
        int id = req["pin_id"] | -1;
        int duty = req["duty"] | 0;
        if (!checkRange(id, PWM_COUNT, res, cmd)) return;
        Hardware.setPwmDuty(id, duty);
        res["status"] = "ok";
        res["duty"] = duty;
    }
    else if (strcmp(cmd, "get_pwm_config") == 0) {
        res["status"] = "ok";
        res["freq"] = Hardware.getFreq();
        res["res"] = Hardware.getRes();
        res["max_duty"] = Hardware.getSafeMaxDuty();
    }
    else if (strcmp(cmd, "set_pwm_config") == 0) {
        int freq = req["freq"] | DEFAULT_PWM_FREQ;
        int r_val = req["res"] | DEFAULT_PWM_RES;
        Hardware.applyPwmConfig(freq, r_val);
        res["status"] = "ok";
    }

    // --- LED 制御 ---
    else if (strcmp(cmd, "set_rgb") == 0) {
        uint8_t r = req["r"] | 0;
        uint8_t g = req["g"] | 0;
        uint8_t b = req["b"] | 0;
        uint8_t br = req["brightness"] | 0;
        AppNet.setLedStatusMode(false); // 手動制御に強制切り替え
        Hardware.setLedColor(r, g, b, br);
        res["status"] = "ok";
        res["r"] = r;
        res["g"] = g;
        res["b"] = b;
        res["brightness"] = br;
    }
    else if (strcmp(cmd, "led_off") == 0) {
        AppNet.setLedStatusMode(false);
        Hardware.setLedColor(0, 0, 0);
        res["status"] = "ok";
    }
    else if (strcmp(cmd, "set_led_mode") == 0) {
        const char* mode = req["mode"] | "";
        if (strcmp(mode, "status") == 0) {
            AppNet.setLedStatusMode(true);
            res["status"] = "ok";
            res["mode"] = "status";
        } else if (strcmp(mode, "manual") == 0) {
            AppNet.setLedStatusMode(false);
            Hardware.setLedColor(0, 0, 0); // manual モード切り替え時は安全のため消灯
            res["status"] = "ok";
            res["mode"] = "manual";
        } else {
            buildError(res, cmd, "ERR_INVALID_MODE", "mode must be 'status' or 'manual'");
        }
    }
    else if (strcmp(cmd, "get_led_state") == 0) {
        uint8_t r, g, b, br;
        Hardware.getLedColor(r, g, b, br);
        res["status"] = "ok";
        res["on"] = (r > 0 || g > 0 || b > 0);
        res["r"] = r;
        res["g"] = g;
        res["b"] = b;
        res["brightness"] = br;
    }

    // --- System Info ---
    else if (strcmp(cmd, "get_status") == 0) {
        res["status"] = "ok";
        res["uptime_ms"] = millis();
        res["free_heap"] = esp_get_free_heap_size();
        res["wifi_connected"] = AppNet.isConnected();
        res["wifi_ip"] = WiFi.localIP().toString();
        res["ap_ip"] = WiFi.softAPIP().toString();
    }
    else if (strcmp(cmd, "ping") == 0) {
        res["status"] = "ok";
        res["message"] = "pong";
    }
    /**
     * @brief Returns a list of available API commands.
     * Self-documenting API.
     */
    else if (strcmp(cmd, "help") == 0) {
        res["status"] = "ok";
        const char* cmds[] = {
            "read_di", "set_do", "get_io_state", "read_adc", 
            "set_pwm", "get_pwm_config", "set_pwm_config",
            "set_rgb", "led_off", "set_led_mode", "get_led_state", 
            "get_status", "ping", "help"
        };
        JsonArray arr = res["commands"].to<JsonArray>();
        for (const char* c : cmds) arr.add(c);
    }
    else {
        buildError(res, cmd, "ERR_UNKNOWN", "Command not found");
    }
}

bool CommandHandler::checkRange(int id, int max, JsonDocument& res, const char* cmd) {
    if (id >= 0 && id < max) return true;
    buildError(res, cmd, "ERR_RANGE", "Invalid pin_id");
    return false;
}

void CommandHandler::buildError(JsonDocument& res, const char* cmd, const char* code, const char* detail) {
    res["status"] = "error";
    res["cmd"] = cmd;
    res["code"] = code;
    res["detail"] = detail;
}