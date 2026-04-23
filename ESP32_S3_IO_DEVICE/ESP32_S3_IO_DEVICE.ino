/**
 * @file ESP32_S3_IO_DEVICE.ino
 * @brief ESP32-S3 リモートIOデバイス ファームウェア
 * 
 * 機能:
 * - シリアル/HTTP API経由のデジタル/アナログIOおよびPWM制御。
 * - ネットワーク設定用のレスポンシブWeb UI。
 * - ステータスLED（ホタル点滅）およびファクトリーリセット。
 */

#include "Config.h"
#include "HardwareManager.h"
#include "NetworkManager.h"
#include "WebHandler.h"
#include "CommandHandler.h"
#include "tusb.h"

/** @brief ハードウェアおよびサービスの初期化。 */
void setup() {
    Serial.begin(115200);
    delay(500); // シリアル初期化待ち
    Serial.println("\n\n=== ESP32-S3 IO DEVICE BOOT ===");
    Hardware.begin();
    AppNet.begin();
    Web.begin();
}

/** @brief メイン処理ループ。USB、Webクライアント、およびシリアルコマンドを処理します。 */
void loop() {
    tud_task(); // CDC用TinyUSBタスク
    Web.handle();
    AppNet.loop();
    
    if (Web.shouldRestart()) {
        delay(1000);
        ESP.restart();
    }

    // シリアルJSONコマンドの処理
    if (Serial.available()) {
        String line = Serial.readStringUntil('\n');
        line.trim();
        if (line.length() > 0) {
            JsonDocument req, res;
            if (deserializeJson(req, line) == DeserializationError::Ok) {
                CommandHandler::process(req, res);
                serializeJson(res, Serial);
                Serial.println();
            }
        }
    }
}
