/**
 * @file CommandHandler.cpp
 * @brief JSONコマンド処理エンジン。
 * 
 * このファイルは、受信したJSON形式のコマンドを解析し、
 * それに対応するハードウェア操作（DIO、ADC、PWM、I2C、LED、センサーなど）を実行します。
 * 処理結果はJSON形式で応答として返されます。
 * @copyright Copyright (c) 2024 norit. Licensed under the MIT License.
 */
#include "CommandHandler.h"
#include "NetworkManager.h"

/**
 * @brief 受信したJSONコマンドを処理し、結果をJSONレスポンスとして返します。
 * コマンドは`req` (リクエスト) から読み取られ、`res` (レスポンス) に結果が書き込まれます。
 * @param req 受信したJSONリクエストデータ。
 * @param res 処理結果を格納するJSONドキュメント。
 */
void CommandHandler::process(JsonVariantConst req, JsonDocument& res) {
    const char* cmd = req["cmd"] | "";
    Serial.printf("Processing command: %s\n", cmd);
    
    // --- DIO / ADC / PWM 制御 ---
    if (strcmp(cmd, "read_di") == 0) {
        int id = req["pin_id"] | -1;
        if (!checkRange(id, DIO_IN_COUNT, res, cmd)) return;
        res["status"] = "ok";
        res["value"] = Hardware.readDI(id);
    }
    else if (strcmp(cmd, "set_do") == 0) { // デジタル出力の設定
        int id = req["pin_id"] | -1;
        int val = req["value"] | 0;
        if (!checkRange(id, DIO_OUT_COUNT, res, cmd)) return;
        Hardware.writeDO(id, val);
        res["status"] = "ok";
    } 
    else if (strcmp(cmd, "get_io_state") == 0) { // すべてのIO状態を一括取得
        // ダッシュボードや監視ツールとの同期に便利なコマンドです。
        // デジタル入力、デジタル出力、ADC、PWMの現在の状態をまとめて返します。
        res["status"] = "ok";
        JsonArray di = res["dio_in"].to<JsonArray>();
        for (int i = 0; i < DIO_IN_COUNT; i++) di.add(Hardware.readDI(i));
        
        JsonArray doo = res["dio_out"].to<JsonArray>();
        for (int i = 0; i < DIO_OUT_COUNT; i++) doo.add(Hardware.readDO(i));

        JsonArray adc = res["adc"].to<JsonArray>();
        for (int i = 0; i < ADC_COUNT; i++) adc.add(Hardware.readADCValue(i));

        JsonArray pwm = res["pwm"].to<JsonArray>();
        for (int i = 0; i < PWM_COUNT; i++) pwm.add(Hardware.getDuty(i));
    }
    else if (strcmp(cmd, "read_adc") == 0) { // ADC値の読み取り
        int id = req["pin_id"] | -1;
        if (!checkRange(id, ADC_COUNT, res, cmd)) return;
        res["status"] = "ok";
        res["value"] = Hardware.readADCValue(id);
    }
    else if (strcmp(cmd, "set_pwm") == 0) { // PWMデューティサイクルの設定
        int id = req["pin_id"] | -1;
        int duty = req["duty"] | 0;
        if (!checkRange(id, PWM_COUNT, res, cmd)) return;
        Hardware.setPwmDuty(id, duty);
        res["status"] = "ok";
        res["duty"] = duty;
    }
    else if (strcmp(cmd, "get_pwm_config") == 0) { // PWM設定の取得
        res["status"] = "ok";
        res["freq"] = Hardware.getFreq();
        res["res"] = Hardware.getRes();
        res["max_duty"] = Hardware.getSafeMaxDuty();
    }
    else if (strcmp(cmd, "set_pwm_config") == 0) { // PWM設定の変更
        int freq = req["freq"] | DEFAULT_PWM_FREQ;
        int r_val = req["res"] | DEFAULT_PWM_RES;
        Hardware.applyPwmConfig(freq, r_val);
        res["status"] = "ok";
    }

    // --- I2C 制御 ---
    else if (strcmp(cmd, "i2c_scan") == 0) { // I2Cデバイスのスキャン
        uint8_t devices[32];
        int count = 0;
        Hardware.scanI2C(devices, count, 32);
        JsonArray arr = res["devices"].to<JsonArray>();
        for (int i = 0; i < count; i++) arr.add(devices[i]);
        res["status"] = "ok";
    }
    else if (strcmp(cmd, "i2c_write") == 0) { // I2Cデバイスへの書き込み
        uint8_t addr = req["addr"] | 0;
        JsonArrayConst data = req["data"].as<JsonArrayConst>();
        if (data.isNull()) {
            buildError(res, cmd, "ERR_PARAM", "data array is required");
            return;
        }
        uint8_t buf[64]; // 最大64バイトの書き込みをサポート
        size_t len = 0;
        for (JsonVariantConst v : data) {
            if (len < 64) buf[len++] = (uint8_t)v.as<int>();
        }
        uint8_t err = Hardware.writeI2C(addr, buf, len);
        res["status"] = (err == 0) ? "ok" : "error";
        res["i2c_err"] = err;
    }
    else if (strcmp(cmd, "i2c_read") == 0) { // I2Cデバイスからの読み取り
        uint8_t addr = req["addr"] | 0;
        int len = req["len"] | 1;
        if (len > 64) len = 64; // 最大64バイトの読み取りをサポート
        uint8_t buf[64];
        uint8_t received = Hardware.readI2C(addr, buf, len);
        JsonArray arr = res["data"].to<JsonArray>();
        for (uint8_t i = 0; i < received; i++) arr.add(buf[i]);
        res["status"] = "ok";
        res["len"] = received;
    }
    else if (strcmp(cmd, "get_sensors") == 0) { // 接続されているセンサーデータの取得
        res["status"] = "ok";
        
        float t, h, p;
        if (Hardware.getBME280Data(t, h, p)) { // BME280センサーデータ
            JsonObject bme = res["bme280"].to<JsonObject>();
            bme["temp"] = t; bme["hum"] = h; bme["press"] = p;
        }

        float acc[3], gyr[3];
        if (Hardware.getMPU6050Data(acc, gyr)) { // MPU6050センサーデータ
            JsonObject mpu = res["mpu6050"].to<JsonObject>();
            JsonArray a = mpu["accel"].to<JsonArray>();
            JsonArray g = mpu["gyro"].to<JsonArray>();
            for(int i=0; i<3; i++) { a.add(acc[i]); g.add(gyr[i]); }
        }

        uint16_t dist;
        if (Hardware.getVL53L0XDistance(dist)) { // VL53L0X距離センサーデータ
            res["vl53l0x"]["distance"] = dist;
        }
    }
    else if (strcmp(cmd, "set_oled") == 0) { // OLEDディスプレイへのテキスト表示
        const char* text = req["text"] | "";
        int x = req["x"] | 0;
        int y = req["y"] | 0;
        int size = req["size"] | 1;
        bool clear = req.containsKey("clear") ? req["clear"].as<bool>() : true;
        Hardware.updateOLED(text, x, y, size, clear);
        res["status"] = "ok";
    }

    // --- LED 制御 ---
    else if (strcmp(cmd, "set_rgb") == 0) { // RGB LEDの色の設定
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
    else if (strcmp(cmd, "led_off") == 0) { // RGB LEDの消灯
        AppNet.setLedStatusMode(false);
        Hardware.setLedColor(0, 0, 0);
        res["status"] = "ok";
    }
    else if (strcmp(cmd, "set_led_mode") == 0) { // RGB LEDの動作モード設定
        const char* mode = req["mode"] | "";
        if (strcmp(mode, "status") == 0) {
            AppNet.setLedStatusMode(true); // WiFiステータス表示モード
            res["status"] = "ok";
            res["mode"] = "status";
        } else if (strcmp(mode, "manual") == 0) {
            AppNet.setLedStatusMode(false); // 手動制御モード
            Hardware.setLedColor(0, 0, 0); // manual モード切り替え時は安全のため消灯
            res["status"] = "ok";
            res["mode"] = "manual";
        } else {
            buildError(res, cmd, "ERR_INVALID_MODE", "mode must be 'status' or 'manual'");
        }
    }
    else if (strcmp(cmd, "get_led_state") == 0) { // RGB LEDの現在の状態を取得
        uint8_t r, g, b, br;
        Hardware.getLedColor(r, g, b, br);
        res["status"] = "ok";
        res["on"] = (r > 0 || g > 0 || b > 0); // いずれかの色が0より大きければONと判断
        res["r"] = r;
        res["g"] = g;
        res["b"] = b;
        res["brightness"] = br;
    }

    // --- システム情報 ---
    else if (strcmp(cmd, "get_status") == 0) { // デバイスのシステムステータスを取得
        res["status"] = "ok";
        res["uptime_ms"] = millis(); // 起動からのミリ秒
        res["free_heap"] = esp_get_free_heap_size(); // 空きヒープメモリサイズ
        res["wifi_connected"] = AppNet.isConnected(); // WiFi接続状態
        res["wifi_ip"] = WiFi.localIP().toString(); // 取得しているIPアドレス
        res["ap_ip"] = WiFi.softAPIP().toString(); // APモードのIPアドレス
    }
    else if (strcmp(cmd, "ping") == 0) { // 疎通確認
        res["status"] = "ok";
        res["message"] = "pong";
    }
    else if (strcmp(cmd, "help") == 0) { // 利用可能なAPIコマンドの一覧
        // このコマンドは、利用可能なすべてのAPIコマンドをリストアップし、
        // 自己文書化されたAPIとして機能します。
        res["status"] = "ok";
        const char* cmds[] = {
            "read_di", "set_do", "get_io_state", "read_adc", 
            "set_pwm", "get_pwm_config", "set_pwm_config",
            "i2c_scan", "i2c_write", "i2c_read",
            "get_sensors", "set_oled",
            "set_rgb", "led_off", "set_led_mode", "get_led_state", 
            "get_status", "ping", "help"
        };
        JsonArray arr = res["commands"].to<JsonArray>();
        for (const char* c : cmds) arr.add(c);
    }
    else { // 未知のコマンド
        buildError(res, cmd, "ERR_UNKNOWN", "Command not found");
    }
}

/**
 * @brief 指定されたIDが有効な範囲内にあるかを確認します。
 * 範囲外の場合、エラーレスポンスを構築します。
 * @param id チェックするID。
 * @param max 許容される最大ID (排他的)。
 * @param res エラーメッセージを格納するJSONドキュメント。
 * @param cmd 実行しようとしたコマンド名。
 * @return IDが有効な範囲内であればtrue、そうでなければfalse。
 */
bool CommandHandler::checkRange(int id, int max, JsonDocument& res, const char* cmd) {
    if (id >= 0 && id < max) return true;
    buildError(res, cmd, "ERR_RANGE", "Invalid pin_id");
    return false;
}

/**
 * @brief エラーメッセージを含むJSONレスポンスを構築します。
 * @param res エラーメッセージを格納するJSONドキュメント。
 * @param cmd エラーが発生したコマンド名。
 * @param code エラーコード (例: "ERR_UNKNOWN", "ERR_PARAM")。
 * @param detail エラーの詳細な説明。
 */
void CommandHandler::buildError(JsonDocument& res, const char* cmd, const char* code, const char* detail) {
    res["status"] = "error";
    res["cmd"] = cmd;
    res["code"] = code;
    res["detail"] = detail;
}