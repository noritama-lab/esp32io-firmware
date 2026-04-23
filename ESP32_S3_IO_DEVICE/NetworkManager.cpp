/**
 * @file NetworkManager.cpp
 * @brief WiFi、mDNS、およびNVS管理の実装。
 * @copyright Copyright (c) 2024 norit. Licensed under the MIT License.
 */
#include "NetworkManager.h"
#include "HardwareManager.h"
#include <ESPmDNS.h>
#include <esp_wifi.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include <esp_mac.h>

// プライベートローカルハンドラのプロトタイプ
void handleFactoryReset();

// WiFiイベント処理用の静的メンバ
void AppNetworkManager::WiFiEvent(WiFiEvent_t event) {
    AppNet.handleWiFiEvent(event);
}
/**
 * @brief ネットワークサービスを起動します。
 * WiFiスタックをリセットし、AP、mDNSを開始し、設定されている場合はSTAに接続します。
 */
void AppNetworkManager::begin() {
    loadConfig();
    
    Serial.begin(115200);
    Serial.println("\n--- System Booting ---");
    Serial.printf("Reset reason: %d\n", esp_reset_reason());

    // 接続速度を優先するため、前回の接続情報をキャッシュとして保持・利用する
    WiFi.persistent(true);
    WiFi.onEvent(AppNetworkManager::WiFiEvent); // イベントハンドラを登録

    // eFuseから読み取ったベースMACアドレスに基づいてユニークな名前を生成
    uint8_t mac[6];
    esp_efuse_mac_get_default(mac); // ハードウェア固有のベースMACアドレスを直接取得
    char suffix[7];
    // 衝突確率を最小限にするため、下位3バイト（24ビット）を使用
    sprintf(suffix, "%02X%02X%02X", mac[3], mac[4], mac[5]);
    _uniqueName = String(DEVICE_NAME_BASE) + "_" + suffix;

    // ネットワークの初期化をシンプルに（過度なMode切替はスタックを不安定にします）
    WiFi.mode(WIFI_AP_STA);
    WiFi.setHostname(_uniqueName.c_str());
    WiFi.softAP(_uniqueName.c_str(), DEFAULT_AP_PASS);
    
    // 省電力モードをオフにすることで、シリアル通信中の切断を防ぐ（最重要）
    WiFi.setAutoReconnect(true);
    WiFi.setSleep(false);
    WiFi.setTxPower(WIFI_POWER_19_5dBm); // 出力を最大に戻して接続の感度を向上させる

    // 接続速度を最大化する設定（キャッシュされたチャンネルを優先的に探す）
    WiFi.setScanMethod(WIFI_FAST_SCAN);
    WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);

    if (MDNS.begin(_uniqueName.c_str())) {
        Serial.println("mDNS responder started");
        MDNS.addService("http", "tcp", 80); // HTTPサービスを登録
    }

    if (_config.wifiEnabled) {
        // 起動直後の即時接続を避け、loop()のタイマーに任せることで
        // 無線チップが安定し、AP(iPhone)が認識しやすくなる時間を稼ぐ
        _reconnectAttemptTime = millis();
    } else {
        setWiFiEnabled(false);
    }
}

void AppNetworkManager::loop() {
    if (_config.ledStatusMode) {
        Hardware.updateStatusLed(_config.wifiEnabled, isConnected());
    }
    handleFactoryReset();

    // 接続が有効で、未接続かつ現在接続試行中でない場合のみ、一定間隔で再接続を試みる
    if (_config.wifiEnabled && !_wifiConnected && !_isConnecting && _config.ssid.length() > 0) {
        if (millis() - _reconnectAttemptTime > WIFI_RECONNECT_INTERVAL_MS) {
            _reconnectAttemptTime = millis();
            reconnect();
        }
    }
}

/**
 * @brief NVS (フラッシュ) からWiFi/ネットワーク設定を読み込みます。
 * 設定が見つからない場合はデフォルト値が適用されます。
 */
void AppNetworkManager::loadConfig() {
    _prefs.begin(PREF_NS, true);
    _config.ssid = _prefs.getString("w_ssid", "");
    _config.pass = _prefs.getString("w_pass", "");
    _config.useStatic = _prefs.getBool("w_static", DEFAULT_WIFI_STATIC);
    _config.ip.fromString(_prefs.getString("w_ip", DEFAULT_WIFI_IP));
    _config.gateway.fromString(_prefs.getString("w_gw", DEFAULT_WIFI_GATEWAY));
    _config.subnet.fromString(_prefs.getString("w_sub", DEFAULT_WIFI_SUBNET));
    _config.ledStatusMode = _prefs.getBool("w_led", true);
    _config.wifiEnabled = _prefs.getBool("w_en", true);
    _prefs.end();
}

void AppNetworkManager::saveConfig(const WifiConfig& cfg) {
    _prefs.begin(PREF_NS, false);
    _prefs.putString("w_ssid", cfg.ssid);
    _prefs.putString("w_pass", cfg.pass);
    _prefs.putBool("w_static", cfg.useStatic);
    _prefs.putString("w_ip", cfg.ip.toString());
    _prefs.putString("w_gw", cfg.gateway.toString());
    _prefs.putString("w_sub", cfg.subnet.toString());
    _prefs.putBool("w_led", cfg.ledStatusMode);
    _prefs.putBool("w_en", cfg.wifiEnabled);
    _prefs.end();
    _config = cfg;
}

/**
 * @brief WiFi接続試行を開始します。
 * この関数は begin() 内および再接続ロジックから呼び出されます。
 */
void AppNetworkManager::connect() {
    reconnect(); // 再接続ロジックに委譲
}

/**
 * @brief WiFi再接続を処理する内部メソッド。
 */
void AppNetworkManager::reconnect() {
    // 重複呼び出し防止
    if (_isConnecting || WiFi.status() == WL_CONNECTED) return;
    if (_config.ssid.length() == 0) return;
    
    // すでに内部で接続試行が走っている場合に設定を上書きしようとするとエラーが出るため、
    // 状態をリセットしてから開始する。
    if (WiFi.status() == WL_DISCONNECTED) {
        WiFi.disconnect(false);
    }

    if (_config.useStatic) {
        WiFi.config(_config.ip, _config.gateway, _config.subnet);
    }

    _isConnecting = true;
    Serial.print("Connecting to WiFi: ");
    Serial.println(_config.ssid);

    // 接続開始。ここからイベント待ちになる
    WiFi.begin(_config.ssid.c_str(), _config.pass.c_str());
}

/**
 * @brief 各種WiFiイベントを処理します。
 */
void AppNetworkManager::handleWiFiEvent(WiFiEvent_t event) {
    switch (event) {
        case ARDUINO_EVENT_WIFI_STA_START:
            Serial.println("WiFi STA Started.");
            break;
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            Serial.println("WiFi STA Disconnected.");
            _wifiConnected = false;
            _isConnecting = false; 
            // 切断時はフラグを下ろして、loop()による次回の再試行を許可する
            break;
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            Serial.print("WiFi STA Connected. IP: ");
            Serial.println(WiFi.localIP());
            _wifiConnected = true;
            _isConnecting = false;
            _reconnectAttemptTime = 0; // 接続成功時にタイマーをリセット
            break;
        case ARDUINO_EVENT_WIFI_AP_START:
            Serial.println("WiFi AP Started.");
            break;
        default: break;
    }
}

/**
 * @brief ファクトリーリセットのためにBOOTボタンを監視します。
 * 5秒間長押しされると実行されます。赤色LEDの点滅で視覚的なフィードバックを提供します。
 */
void handleFactoryReset() {
    static unsigned long start = 0;
    
    // 起動直後（5秒以内）はシリアル接続によるノイズを拾いやすいため、判定をスキップ
    if (millis() < 5000) return;

    if (digitalRead(BOOT_BUTTON_PIN) == LOW) { // ボタン押下
        if (start == 0) start = millis();
        if (millis() - start > FACTORY_RESET_TIME) {
            // 視覚的フィードバック: 高速赤色点滅
            for(int i=0; i<10; i++) {
                Hardware.setLedColor(255, 0, 0, 10); delay(100);
                Hardware.setLedColor(0, 0, 0, 0); delay(100);
            }
            Preferences p; p.begin(PREF_NS, false); p.clear(); p.end();
            ESP.restart();
        }
    } else {
        start = 0;
    }
}

/**
 * @brief Wi-Fiの有効/無効を切り替える。無効時は無線チップをオフにして電力と干渉を抑える。
 */
void AppNetworkManager::setWiFiEnabled(bool enabled) {
    _config.wifiEnabled = enabled;

    if (!enabled) {
        Serial.println("Wi-Fi STA Disabled. AP Only mode active.");
        WiFi.disconnect(true);
        WiFi.mode(WIFI_AP); // STAを無効化し、APモードのみを維持
        _wifiConnected = false;
    } else {
        Serial.println("Wi-Fi Mode Enabled. Restoring Network services.");
        WiFi.mode(WIFI_AP_STA);
        connect();
    }
}

AppNetworkManager AppNet;

/**
 * @brief Returns the current WiFi connection status.
 */
bool AppNetworkManager::isConnected() const {
    return _wifiConnected;
}

/**
 * @brief Sets the LED status mode and saves it to NVS.
 */
void AppNetworkManager::setLedStatusMode(bool mode) {
    // 現在のモードと同じなら、書き込みを行わずに復帰する
    if (_config.ledStatusMode == mode) return;

    _config.ledStatusMode = mode;
    saveConfig(_config);
}