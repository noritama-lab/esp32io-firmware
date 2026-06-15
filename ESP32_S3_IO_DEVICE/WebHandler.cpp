/**
 * @file WebHandler.cpp
 * @brief UIおよびAPIエンドポイント用のWebサーバー実装。
 * 
 * このファイルは、デバイスのWebインターフェースとRESTful APIエンドポイントを管理します。
 * ユーザーが設定を変更したり、デバイスの状態を監視したりするためのWebページを提供し、
 * 外部システムからのコマンドを受け付けるAPIも実装しています。
 * @copyright Copyright (c) 2024 norit. Licensed under the MIT License.
 */
#include "WebHandler.h"
#include "NetworkManager.h"
#include "CommandHandler.h"
#include <ArduinoJson.h>
#include <WiFi.h> // WiFi.localIP()などのために必要

static WebServer server(80);

/**
 * @brief Webサーバーを初期化し、ルート、保存、APIエンドポイントを設定します。
 * サーバーを開始し、クライアントからのリクエストをリッスンする準備をします。
 */
void WebHandler::begin() {
    server.on("/", HTTP_GET, [this]() { handleRoot(); });
    server.on("/save", HTTP_POST, [this]() { handleSave(); });
    server.on("/api", HTTP_ANY, [this]() { handleApi(); });
    server.begin();
    Serial.println("Web server started on port 80.");
}

/** @brief Webサーバーのクライアントリクエストを処理し、必要に応じてデバイスを再起動します。 */
void WebHandler::handle() {
    server.handleClient();
}

/**
 * @brief ルートパス('/')へのGETリクエストを処理し、設定ページを送信します。
 */
void WebHandler::handleRoot() {
    server.send(200, "text/html", makeConfigPage());
}

/**
 * @brief 設定フォームからのPOSTリクエストを処理し、設定を保存してデバイスを再起動します。
 */
void WebHandler::handleSave() {
    if (!server.hasArg("ssid")) {
        server.send(400, "text/plain", "SSID is required");
        return;
    }

    WifiConfig cfg = AppNet.getConfig(); // 現在の設定をコピー
    cfg.ssid = server.arg("ssid");
    // パスワード入力がある場合のみ更新する
    if (server.arg("pass").length() > 0) {
        cfg.pass = server.arg("pass");
    }
    cfg.useStatic = (server.arg("ip_mode") == "static");
    cfg.ip.fromString(server.arg("ip"));
    cfg.gateway.fromString(server.arg("gateway"));
    cfg.subnet.fromString(server.arg("subnet"));
    cfg.ledStatusMode = (server.arg("led_mode") == "status");
    cfg.wifiEnabled = (server.arg("wifi_en") == "1");
    cfg.dioOutInverted = (server.arg("dio_inv") == "1");

    AppNet.saveConfig(cfg);

    String html = "<html><body><h3>Settings Saved</h3><p>Device is restarting in 1 sec...</p></body></html>";
    server.send(200, "text/html", html);
    _pendingRestart = true;
}

/**
 * @brief HTTPリクエストをCommandHandlerに橋渡しします。
 * POSTリクエストの場合はJSONボディを解析し、GETリクエストの場合はクエリパラメータを解析して
 * コマンドハンドラに渡します。結果はJSON形式でクライアントに返されます。
 */
void WebHandler::handleApi() {
    JsonDocument req, res;
    // ArduinoJsonのメモリプールサイズを確保。
    // 複雑なリクエストやレスポンスに対応するため、適切なサイズを設定する。
    // ここでは256バイトを例としていますが、実際の使用状況に応じて調整してください。
    
    if (server.method() == HTTP_POST && server.hasArg("plain")) {
        deserializeJson(req, server.arg("plain"));
    } else {
        // クエリパラメータを解析。数値を検出してキャストします。
        for (int i = 0; i < server.args(); i++) {
            String name = server.argName(i);
            String val = server.arg(i);
            if (val.length() > 0 && (isdigit(val[0]) || (val[0] == '-' && val.length() > 1 && isdigit(val[1])))) {
                req[name] = val.toInt();
            } else {
                req[name] = val;
            }
        }
    }

    CommandHandler::process(req, res);

    String response;
    serializeJson(res, response);
    server.send(200, "application/json", response);
}

/**
 * @brief ネットワーク設定ページをHTML形式でレンダリングします。
 * 現在の設定値に基づいてフォームを生成し、モダンでレスポンシブなCSSデザインを適用します。
 * センサーデータのリアルタイム更新のためのJavaScriptも含まれます。
 */
String WebHandler::makeConfigPage() {
    const WifiConfig& cfg = AppNet.getConfig();
    String html;
    html.reserve(4096); // メモリ断片化を防ぐためにバッファを事前割り当て
    html += "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width,initial-scale=1.0'>";
    html += "<style>";
    html += "body{font-family:-apple-system,sans-serif;background:#f0f2f5;color:#1c1e21;margin:0;padding:20px;}";
    html += ".card{background:#fff;padding:24px;border-radius:12px;box-shadow:0 4px 12px rgba(0,0,0,0.1);max-width:440px;margin:0 auto;}";
    html += "h1{font-size:22px;margin:0 0 20px;color:#1877f2;text-align:center;}";
    html += "label{display:block;margin:12px 0 6px;font-weight:bold;font-size:14px;}";
    html += "input[type='text'],input[type='password']{width:100%;padding:10px;border:1px solid #ddd;border-radius:6px;box-sizing:border-box;}";
    html += ".row{display:flex;gap:15px;margin:10px 0;} .row label{margin:0;font-weight:normal;}";
    html += ".static-group{background:#f9f9f9;padding:12px;border-radius:8px;margin-top:10px;display:" + String(cfg.useStatic ? "block" : "none") + ";}";
    html += "button{width:100%;padding:12px;background:#1877f2;color:#fff;border:none;border-radius:6px;font-weight:bold;margin-top:10px;cursor:pointer;}";
    html += "button:hover{background:#166fe5;} .status, .monitor{margin-top:20px;font-size:13px;color:#65676b;border-top:1px solid #eee;padding-top:15px;}";
    html += ".sensor-val{background:#e7f3ff;padding:4px 8px;border-radius:4px;color:#1877f2;font-weight:bold;display:inline-block;margin:2px;}";
    html += "</style>";
    html += "<script>";
    html += "function toggleStatic(show){document.getElementById('static_fields').style.display=show?'block':'none';}";
    html += "async function updateSensors(){try{const r=await fetch('/api?cmd=get_sensors');const d=await r.json();let h='';";
    html += "if(d.bme280)h+='<div>BME280: <span class=\"sensor-val\">'+d.bme280.temp.toFixed(1)+'℃</span><span class=\"sensor-val\">'+d.bme280.hum.toFixed(1)+'%</span></div>';";
    html += "if(d.mpu6050)h+='<div>MPU6050 Accel: <span class=\"sensor-val\">X:'+d.mpu6050.accel[0].toFixed(1)+'</span></div>';";
    html += "if(d.vl53l0x)h+='<div>VL53L0X: <span class=\"sensor-val\">'+d.vl53l0x.distance+'mm</span></div>';";
    html += "document.getElementById('m').innerHTML=h||'No sensors connected';}catch(e){}}";
    html += "setInterval(updateSensors,2000);";
    html += "</script>";
    html += "</head><body><div class='card'>";
    
    html += "<h1>Sensor Monitor</h1><div id='m' class='monitor'>Loading sensors...</div>";
    html += "<div style='height:20px;border-bottom:2px dashed #eee;margin-bottom:20px;'></div>";

    html += "<h1>" + AppNet.getDeviceName() + "</h1>";
    html += "<form method='POST' action='/save'>";
    
    html += "<label>WiFi SSID</label><input name='ssid' type='text' value='" + cfg.ssid + "' placeholder='SSID'>";
    html += "<label>WiFi Password</label><input name='pass' type='password' placeholder='Leave empty to keep current'>";
    
    html += "<label>Wireless Radio</label><div class='row'>";
    html += "<label><input type='radio' name='wifi_en' value='1' " + String(cfg.wifiEnabled ? "checked" : "") + "> Enabled</label>";
    html += "<label><input type='radio' name='wifi_en' value='0' " + String(cfg.wifiEnabled ? "" : "checked") + "> AP Only (STA Disabled)</label>";
    html += "</div>";

    html += "<label>IP Addressing</label><div class='row'>";
    html += "<label><input type='radio' name='ip_mode' value='dhcp' " + String(cfg.useStatic ? "" : "checked") + " onclick='toggleStatic(false)'> DHCP</label>";
    html += "<label><input type='radio' name='ip_mode' value='static' " + String(cfg.useStatic ? "checked" : "") + " onclick='toggleStatic(true)'> Static IP</label>";
    html += "</div>";

    html += "<div id='static_fields' class='static-group'>";
    html += "<label>IP Address</label><input name='ip' type='text' value='" + cfg.ip.toString() + "'>";
    html += "<label>Gateway</label><input name='gateway' type='text' value='" + cfg.gateway.toString() + "'>";
    html += "<label>Subnet</label><input name='subnet' type='text' value='" + cfg.subnet.toString() + "'>";
    html += "</div>";

    html += "<label>RGB LED Mode</label><div class='row'>";
    html += "<label><input type='radio' name='led_mode' value='status' " + String(cfg.ledStatusMode ? "checked" : "") + "> WiFi Status</label>";
    html += "<label><input type='radio' name='led_mode' value='manual' " + String(cfg.ledStatusMode ? "" : "checked") + "> Manual Control</label>";
    html += "</div>";

    html += "<label>DIO Output Logic</label><div class='row'>";
    html += "<label><input type='radio' name='dio_inv' value='0' " + String(cfg.dioOutInverted ? "" : "checked") + "> Active High (ON=High)</label>";
    html += "<label><input type='radio' name='dio_inv' value='1' " + String(cfg.dioOutInverted ? "checked" : "") + "> Active Low (ON=Low)</label>";
    html += "</div>";

    html += "<button type='submit'>Save & Apply Settings</button>";
    html += "</form>";
    
    html += "<div class='status'>";
    html += "Current IP: <b>" + WiFi.localIP().toString() + "</b><br>";
    html += "AP Address: <b>" + WiFi.softAPIP().toString() + "</b><br>";
    html += "Uptime: " + String(millis() / 1000) + " seconds";
    html += "</div></div></body></html>";
    
    return html;
}

WebHandler Web;