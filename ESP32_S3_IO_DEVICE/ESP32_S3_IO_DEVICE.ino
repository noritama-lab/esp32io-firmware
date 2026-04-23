/**
 * @file ESP32_S3_IO_DEVICE_NET.ino
 * @brief ESP32-S3 Remote IO Device Firmware
 * 
 * Features:
 * - Digital/Analog IO & PWM control over Serial/HTTP API.
 * - Responsive Web UI for Network Configuration.
 * - Breathing status LED & Factory reset support.
 */

#include "Config.h"
#include "HardwareManager.h"
#include "NetworkManager.h"
#include "WebHandler.h"
#include "CommandHandler.h"
#include "tusb.h"

/** @brief Hardware and Service Initialization. */
void setup() {
    Serial.begin(115200);
    Hardware.begin();
    AppNet.begin();
    Web.begin();
}

/** @brief Main processing loop. Handles USB, Web Clients, and Serial commands. */
void loop() {
    tud_task(); // TinyUSB task for CDC
    Web.handle();
    AppNet.loop();
    
    if (Web.shouldRestart()) {
        delay(1000);
        ESP.restart();
    }

    // Process Serial JSON Commands
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
