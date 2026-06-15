#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <WiFi.h>
#include <Preferences.h>
#include "Config.h"

/**
 * @class AppNetworkManager
 * @brief WiFi接続、mDNS、NVS設定の管理を行います。
 * 
 * このクラスは、ESP32のWiFiステーションモードとアクセスポイントモード、
 * mDNSサービス、および不揮発性ストレージ (NVS) を介した設定の永続化を管理します。
 * ネットワーク関連のイベント処理と再接続ロジックも含まれています。
 */
class AppNetworkManager {
public:
    /**
     * @brief ネットワークサービスを開始し、WiFiとmDNSを初期化します。
     */
    void begin();
    /**
     * @brief ネットワークマネージャーの定期的な処理を実行します。
     * Arduinoのloop()関数内で定期的に呼び出す必要があります。
     */
    void loop();
    /**
     * @brief 現在のWiFi設定をNVSに保存します。
     * @param cfg 保存するWifiConfig構造体。
     */
    void saveConfig(const WifiConfig& cfg);
    /**
     * @brief 現在のWiFi設定を取得します。
     * @return 現在のWifiConfig構造体への定数参照。
     */
    const WifiConfig& getConfig() const { return _config; }
    /**
     * @brief デバイスのユニークな名前を取得します。
     * @return デバイスのユニークな名前を表すStringオブジェクト。
     */
    String getDeviceName() const { return _uniqueName; }
    /**
     * @brief LEDのステータス表示モードを設定します。
     * @param mode trueの場合、LEDはWiFi接続ステータスを表示します。falseの場合、手動制御モードになります。
     */
    void setLedStatusMode(bool mode);
    /**
     * @brief WiFi機能を有効または無効にします。
     * @param enabled trueの場合WiFiを有効にし、falseの場合無効にします。
     */
    void setWiFiEnabled(bool enabled);
    /**
     * @brief WiFiが現在接続されているかどうかを返します。
     * @return trueの場合WiFiに接続されており、falseの場合接続されていません。
     */
    bool isConnected() const;
    /**
     * @brief WiFiイベントを処理するための静的コールバック関数。
     * @param event 発生したWiFiイベントのタイプ。
     */
    static void WiFiEvent(WiFiEvent_t event);

private:
    String _uniqueName;
    WifiConfig _config;
    Preferences _prefs;
    bool _wifiConnected = false; /**< WiFiがアクセスポイントに接続されているかを示すフラグ。 */
    bool _isConnecting = false;  /**< WiFi接続試行中であるかを示すフラグ。 */
    bool _wifiEnabled = true;    /**< WiFi機能が全体として有効であるかを示すフラグ。 */
    unsigned long _reconnectAttemptTime = 0; /**< 次のWiFi再接続試行までのタイマー。 */

    void loadConfig(); /**< NVSからWiFi/ネットワーク設定を読み込みます。 */
    void connect();    /**< WiFi接続試行を開始します。 */
    void reconnect();  /**< WiFiへの再接続を試みます。 */
    void handleWiFiEvent(WiFiEvent_t event); /**< 各種WiFiイベントを処理します。 */
};

extern AppNetworkManager AppNet;
#endif