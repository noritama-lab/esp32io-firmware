/**
 * @file WebHandler.cpp
 * @brief Web Server implementation for UI and API endpoints.
 * @copyright Copyright (c) 2024 norit. Licensed under the MIT License.
 */
#include "WebHandler.h"
#include "NetworkManager.h"
#include "CommandHandler.h"
#include <ArduinoJson.h>

static WebServer server(80);

/**
 * @brief Sets up Web Server routes.
 */
void WebHandler::begin() {
    server.on("/", HTTP_GET, [this]() { handleRoot(); });
    server.on("/save", HTTP_POST, [this]() { handleSave(); });
    server.on("/api", HTTP_ANY, [this]() { handleApi(); });
    server.begin();
}

/** @brief Must be called in main loop to handle clients. */
void WebHandler::handle() {
    server.handleClient();
}

void WebHandler::handleRoot() {
    server.send(200, "text/html", makeConfigPage());
}

void WebHandler::handleSave() {
    if (!server.hasArg("ssid")) {
        server.send(400, "text/plain", "SSID is required");
        return;
    }

    WifiConfig cfg;
    cfg.ssid = server.arg("ssid");
    cfg.pass = server.arg("pass");
    cfg.useStatic = (server.arg("ip_mode") == "static");
    cfg.ip.fromString(server.arg("ip"));
    cfg.gateway.fromString(server.arg("gateway"));
    cfg.subnet.fromString(server.arg("subnet"));
    cfg.ledStatusMode = (server.arg("led_mode") == "status");

    AppNet.saveConfig(cfg);

    String html = "<html><body><h3>Settings Saved</h3><p>Device is restarting in 1 sec...</p></body></html>";
    server.send(200, "text/html", html);
    delay(1000);
    ESP.restart();
}

/**
 * @brief Bridges HTTP requests to the CommandHandler.
 * Supports POST JSON or GET query parameters.
 */
void WebHandler::handleApi() {
    JsonDocument req, res;
    
    if (server.method() == HTTP_POST && server.hasArg("plain")) {
        deserializeJson(req, server.arg("plain"));
    } else {
        // Parse query params. Detect and cast numeric values.
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
 * @brief Renders the Network Configuration Page.
 * Uses a modern responsive CSS design.
 */
String WebHandler::makeConfigPage() {
    const WifiConfig& cfg = AppNet.getConfig();
    String html;
    html.reserve(4096); // Pre-allocate buffer to prevent memory fragmentation
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
    html += "button{width:100%;padding:12px;background:#1877f2;color:#fff;border:none;border-radius:6px;font-weight:bold;margin-top:20px;cursor:pointer;}";
    html += "button:hover{background:#166fe5;} .status{margin-top:20px;font-size:13px;color:#65676b;border-top:1px solid #eee;padding-top:15px;}";
    html += "</style>";
    html += "<script>function toggleStatic(show){document.getElementById('static_fields').style.display=show?'block':'none';}</script>";
    html += "</head><body><div class='card'>";
    html += "<h1>" + AppNet.getDeviceName() + "</h1>";
    html += "<form method='POST' action='/save'>";
    
    html += "<label>WiFi SSID</label><input name='ssid' type='text' value='" + cfg.ssid + "' placeholder='SSID'>";
    html += "<label>WiFi Password</label><input name='pass' type='password' placeholder='Leave empty to keep current'>";
    
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