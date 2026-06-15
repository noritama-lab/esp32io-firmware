/**
 * @file ESP32_S3_IO_DEVICE.ino
 * @brief ESP32-S3 リモートIOデバイス ファームウェア
 * 
 * このファームウェアは、ESP32-S3を多機能なリモートIOデバイスとして機能させます。
 * HTTP APIまたはシリアル通信を介して、デジタルIO、アナログ入力、PWM制御、
 * I2Cセンサーの読み取り、OLEDディスプレイへの表示、内蔵RGB LEDの制御が可能です。
 * また、Web UIによるネットワーク設定とデバイス管理機能も提供します。
 * 
 * 機能:
 * - シリアル/HTTP API経由のデジタル/アナログIOおよびPWM制御。
 * - ネットワーク設定用のレスポンシブWeb UI。
 * - ステータスLED（ホタル点滅）およびファクトリーリセット。
 */
// 必要なライブラリとカスタムヘッダーファイルをインクルード
#include "Config.h"
#include "HardwareManager.h"
#include "NetworkManager.h"
#include "WebHandler.h"
#include "CommandHandler.h"
#include "tusb.h"

/**
 * @brief デバイスの初期設定を行います。
 * シリアル通信の開始、ネットワークマネージャー、ハードウェアマネージャー、
 * およびWebサーバーの初期化が含まれます。
 */
void setup() {
    Serial.begin(115200);
    delay(500); 
    Serial.println("\n\n=== ESP32-S3 IO DEVICE BOOT ===");
    Serial.printf("Reset reason: %d\n", esp_reset_reason());

    // 1. ネットワークマネージャーを初期化し、NVSから設定をロード
    AppNet.begin(); 
    // 2. ハードウェアマネージャーを初期化し、ピン設定と周辺機器を準備
    Hardware.begin(); 
    // 3. Webサーバーを初期化し、Web UIとAPIエンドポイントを有効化
    Web.begin();
}

/** @brief メイン処理ループ。USB、Webクライアント、およびシリアルコマンドを処理します。 */
void loop() {
    tud_task(); // USB CDC (シリアル) タスクを処理
    Web.handle(); // Webサーバーのクライアントリクエストを処理
    AppNet.loop(); // ネットワークマネージャーの定期処理 (WiFi再接続、LED更新など)

    // WebUIからの設定保存などにより再起動が要求された場合
    if (Web.shouldRestart()) {
        delay(1000);
        ESP.restart();
    }

    // シリアルポートからのJSONコマンドを処理
    // 新しい行が利用可能であれば読み取り、JSONとして解析してCommandHandlerに渡す
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
