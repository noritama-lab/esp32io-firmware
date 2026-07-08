#ifndef WEB_HANDLER_H
#define WEB_HANDLER_H

#include <WebServer.h>
#include "Config.h"

/**
 * @class WebHandler
 * @brief WebサーバーとUIの管理を担当します。
 * 
 * このクラスは、ESP32-S3デバイスのWebインターフェースとRESTful APIエンドポイントを
 * 設定および処理するための機能を提供します。
 */
class WebHandler {
public:
    /**
     * @brief Webサーバーを初期化し、リクエストハンドラを設定します。
     */
    void begin();
    /**
     * @brief Webサーバーのクライアントリクエストを処理します。
     * Arduinoのloop()関数内で定期的に呼び出す必要があります。
     */
    void handle();
    /**
     * @brief デバイスの再起動が必要かどうかを返します。
     * @return trueの場合、デバイスは再起動を保留しています。
     */
    bool shouldRestart() const { return _pendingRestart; }

private:
    void handleRoot(); /**< ルートパス('/')へのGETリクエストを処理します。 */
    void handleSave(); /**< 設定フォームからのPOSTリクエストを処理します。 */
    void handleApi();  /**< APIエンドポイントへのリクエストを処理します。 */
    String makeConfigPage(); /**< 設定ページのHTMLコンテンツを生成します。 */
    bool _pendingRestart = false; /**< デバイスの再起動が保留中であるかを示すフラグ。 */
};

extern WebHandler Web;
#endif