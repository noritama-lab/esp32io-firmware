/**
 * @file NetworkManager.cpp
 * @brief WiFi、mDNS、およびNVS管理の実装。
 * 
 * このファイルは、ESP32のWiFi接続（ステーションモードおよびアクセスポイントモード）、
 * mDNSサービス、および不揮発性ストレージ (NVS) を介した設定の永続化を管理します。
 * ネットワーク関連のイベント処理と再接続ロジックも含まれています。
 * @copyright Copyright (c) 2024 norit. Licensed under the MIT License.
 */
#include "NetworkManager.h"
#include "HardwareManager.h"
#include <ESPmDNS.h>
#include <esp_wifi.h>
#include <esp_mac.h>

/**
 * @brief ファクトリーリセット処理のプロトタイプ宣言。
 * BOOTボタンの長押しを監視し、NVS設定をクリアしてデバイスを再起動します。
 */
void handleFactoryReset();

/**
 * @brief WiFiイベントを処理するための静的コールバック関数。
 * @param event 発生したWiFiイベントのタイプ。
 */
void AppNetworkManager::WiFiEvent(WiFiEvent_t event) {
    AppNet.handleWiFiEvent(event);
}

/**
 * @brief ネットワークサービスを開始します。
 * NVSから設定をロードし、WiFi（APおよびSTAモード）、mDNSを初期化します。
 */
void AppNetworkManager::begin() {
    loadConfig();
    
    // 接続速度を優先するため、前回の接続情報をキャッシュとして保持・利用する
    WiFi.persistent(true);
    WiFi.onEvent(AppNetworkManager::WiFiEvent); // イベントハンドラを登録

    // eFuseから読み取ったベースMACアドレスに基づいてユニークな名前を生成
    uint8_t mac[6];
    esp_efuse_mac_get_default(mac); // ハードウェア固有のベースMACアドレスを直接取得
    char suffix[7];
    // 衝突確率を最小限にするため、MACアドレスの下位3バイト（24ビット）を使用
    sprintf(suffix, "%02X%02X%02X", mac[3], mac[4], mac[5]);
    _uniqueName = String(DEVICE_NAME_BASE) + "_" + suffix;

    // WiFiモードをAP+STAに設定し、ホスト名とAPのSSIDを設定
    WiFi.mode(WIFI_AP_STA);
    WiFi.setHostname(_uniqueName.c_str());
    WiFi.softAP(_uniqueName.c_str(), DEFAULT_AP_PASS);
    
    // WiFiの安定性とパフォーマンスを向上させるための設定
    WiFi.setAutoReconnect(true);
    WiFi.setSleep(false); // 省電力モードを無効化し、通信の安定性を優先
    WiFi.setTxPower(WIFI_POWER_19_5dBm); // 送信電力を最大に設定し、接続範囲と安定性を向上

    // 接続速度を最適化するためのスキャンおよびソート方法の設定
    WiFi.setScanMethod(WIFI_FAST_SCAN); // 高速スキャンを有効化
    WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL); // 信号強度に基づいてAPをソート

    if (MDNS.begin(_uniqueName.c_str())) {
        Serial.println("mDNS responder started");
        MDNS.addService("http", "tcp", 80); // HTTPサービスを登録
    }

    if (_config.wifiEnabled) {
        // 起動直後のSTA接続は、無線チップの安定化とAPの認識時間を考慮し、
        // loop()内のタイマーによって遅延させて開始します。
        _reconnectAttemptTime = millis();
    } else {
        setWiFiEnabled(false);
    }
}

void AppNetworkManager::loop() {
    // LEDステータスモードが有効な場合、WiFi接続状態に応じてLEDを更新
    if (_config.ledStatusMode) {
        Hardware.updateStatusLed(_config.wifiEnabled, isConnected());
    }
    // ファクトリーリセットのボタン監視
    handleFactoryReset();

    // WiFiが有効で、未接続、かつ現在接続試行中でない場合に、
    // 一定間隔で再接続を試みます。
    if (_config.wifiEnabled && !_wifiConnected && !_isConnecting && _config.ssid.length() > 0) {
        if (millis() - _reconnectAttemptTime > WIFI_RECONNECT_INTERVAL_MS) {
            _reconnectAttemptTime = millis();
            reconnect();
        }
    }
}

/**
 * @brief NVS (不揮発性ストレージ) からWiFiおよびネットワーク設定を読み込みます。
 * 設定が見つからない場合、または初めての起動の場合は、デフォルト値が適用されます。
 * @note 設定は`_config`メンバ変数に格納されます。
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
    _config.dioOutInverted = _prefs.getBool("dio_inv", DEFAULT_DIO_OUT_INVERTED);
    _prefs.end();
}

/**
 * @brief 現在のWiFiおよびネットワーク設定をNVS (不揮発性ストレージ) に保存します。
 * @param cfg 保存するWifiConfig構造体。
 */
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
    _prefs.putBool("dio_inv", cfg.dioOutInverted);
    _prefs.end();
    _config = cfg;
}

/**
 * @brief WiFi接続試行を開始します。
 * 内部的に`reconnect()`メソッドを呼び出します。
 */
void AppNetworkManager::connect() {
    reconnect();
}

/**
 * @brief WiFiへの再接続を試みます。
 * 既に接続中または接続試行中の場合は何もしません。
 * 静的IP設定が有効な場合は、その設定を適用します。
 */
void AppNetworkManager::reconnect() {
    // 重複呼び出し防止
    if (_isConnecting || WiFi.status() == WL_CONNECTED) return;
    if (_config.ssid.length() == 0) return;
    
    // すでに内部で接続試行が走っている場合に設定を上書きしようとするとエラーが出るため、
    // 状態をリセットしてから開始する。
    if (WiFi.status() == WL_DISCONNECTED || WiFi.status() == WL_NO_SHIELD) { // WL_NO_SHIELDも考慮
        WiFi.disconnect(false);
    }

    if (_config.useStatic) {
        WiFi.config(_config.ip, _config.gateway, _config.subnet);
    }

    _isConnecting = true;
    Serial.print("Connecting to WiFi: ");
    Serial.println(_config.ssid);

    // WiFi接続を開始。結果はイベントハンドラで処理されます。
    WiFi.begin(_config.ssid.c_str(), _config.pass.c_str());
}

/**
 * @brief 各種WiFiイベントを処理します。
 * WiFiの接続、切断、IPアドレス取得などのイベントに応じて、内部状態を更新します。
 * @param event 発生したWiFiイベントのタイプ。
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
 * @brief BOOTボタンの長押しを監視し、ファクトリーリセットを実行します。
 * BOOTボタンが`FACTORY_RESET_TIME`（デフォルト5秒）以上長押しされた場合、
 * NVSに保存されているすべての設定をクリアし、デバイスを再起動します。
 * 視覚的なフィードバックとして、内蔵RGB LEDが赤色で高速点滅します。
 */
void handleFactoryReset() {
    static unsigned long start = 0;
    
    // 起動直後（5秒以内）は誤検出を防ぐため、ボタン監視をスキップします。
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
 * @brief WiFi機能を有効または無効にします。
 * WiFiを無効にすると、STAモードが停止され、APモードのみが維持されます。
 * これにより、電力消費と無線干渉を低減できます。
 * @param enabled trueの場合WiFiを有効にし、falseの場合無効にします。
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
 * @brief 現在のWiFi接続状態を返します。
 * @return trueの場合WiFiに接続されており、falseの場合接続されていません。
 */
bool AppNetworkManager::isConnected() const {
    return _wifiConnected;
}

/**
 * @brief LEDの動作モード（システムステータス表示または手動制御）を設定し、NVSに保存します。
 * @param mode trueの場合、LEDはWiFi接続ステータスを表示します。
 *             falseの場合、LEDはAPI経由での手動制御モードになります。
 */
void AppNetworkManager::setLedStatusMode(bool mode) {
    // 現在のモードと同じであれば、NVSへの書き込みをスキップします。
    if (_config.ledStatusMode == mode) return;

    _config.ledStatusMode = mode;
    saveConfig(_config);
}