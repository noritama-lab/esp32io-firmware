# ESP32-S3 Remote I/O Firmware  
JSON-based USB Serial and HTTP Remote I/O firmware for ESP32-S3.

[日本語](#日本語版) | [English](#english-version)

---

# 日本語版

> ⚠️ **開発終了のお知らせ**
> 本リポジトリ（HTTP版）の開発は終了しました。今後は MQTT版へ移行してください。
> → https://github.com/noritama-lab/esp32io-mqtt-firmware

## 概要

**ESP32-S3 Remote I/O Firmware** は、USBシリアルまたは HTTP 経由で  
**JSON コマンドを受け取り、I/O を制御できる汎用リモートI/Oファームウェア** です。

Python ライブラリ **ESP32IO** だけでなく、  
**Excel VBA / Node-RED / Power Automate / curl / JavaScript / C# / Go / どんな環境からでも**  
JSON を送るだけで ESP32-S3 の I/O を操作できます。

> **→ JSON を送れる環境なら何でも使える。**

関連プロジェクト:  
- https://github.com/Noritama-Lab/esp32io-api

---

## 主な機能

- 6x デジタル入力  
- 6x デジタル出力  
- 2x アナログ入力 (ADC)  
- 2x PWM 出力  
- USB CDC JSON API  
- HTTP JSON API  
- Wi-Fi STA & AP モード  
- mDNS サポート  
- NeoPixel 状態表示 LED (GPIO38)  
- I2C 拡張サポート  
- BME280 / MPU6050 / VL53L1X / SSD1306 対応  
- BOOTボタン長押しでファクトリーリセット  
- NVS による設定保存  

---

## ハードウェア設定 & ピンマッピング

| 機能タイプ | pin_id | GPIO |
|---|---|---|
| Digital Input | 0–5 | 4, 5, 6, 7, 8, 9 |
| Digital Output | 0–5 | 10, 11, 12, 13, 14, 15 |
| ADC Input | 0–1 | 1, 2 |
| PWM Output | 0–1 | 36, 37 |
| Status LED | - | 38 |
| I2C (SDA, SCL) | - | 40, 41 |
| Reset Button | - | 0 (BOOT) |

---

## 必要ライブラリ (Arduino Library Manager)

- ArduinoJson (v7)
- Adafruit BME280 / MPU6050 / SSD1306 / VL53L1X 各ライブラリ

---

## セットアップ手順

1. デバイス起動後、`ESP32_S3_IO_<MACサフィックス>` という名前のWiFi APが立ち上がります（パスワード: `esp32setup`）。
2. AP に接続し、ブラウザで `http://192.168.4.1/` を開きます。
3. WiFi SSID/パスワードなどを入力して保存すると、デバイスが再起動してWiFiに接続します。
4. 接続後は `http://<device_ip>/` または `http://ESP32_S3_IO_XXXXXX.local/` から設定ページとセンサーモニターにアクセスできます。
5. USBシリアル（115200bps）でも、Wi-Fi設定なしにJSONコマンドをそのまま送受信できます。

---

## API リファレンス（USB / HTTP 共通）

すべてのコマンドは以下形式です。

```json
{"cmd":"command_name", ...}
```

コマンド一覧（`read_di` / `set_do` / `get_io_state` / `read_adc` / `set_pwm` / `get_pwm_config` / `set_pwm_config` / `i2c_scan` / `i2c_write` / `i2c_read` / `get_sensors` / `set_oled` / `set_rgb` / `led_off` / `set_led_mode` / `get_led_state` / `get_status` / `ping` / `help`）とパラメータ・レスポンスの詳細は、
**[COMMAND_REFERENCE.md](COMMAND_REFERENCE.md)** を参照してください。

---

## さまざまな環境から利用可能

### Python
```python
import requests
requests.post("http://device.local/api", json={"cmd":"ping"})
```

### Excel VBA
```vb
result = HttpPostJson("http://device.local/api", "{""cmd"":""ping""}")
```

### curl
```bash
curl -X POST http://device.local/api -d '{"cmd":"ping"}'
```

### JavaScript
```js
fetch("/api", {method:"POST", body:JSON.stringify({cmd:"ping"})})
```

### Node-RED / Power Automate / C# / Go / etc.
> **Any environment capable of sending JSON can control the device.**

---

## ライセンス

MIT License  
Copyright (c) 2026 Noritama-Lab

---

# English Version

> ⚠️ **Development of this repository (HTTP version) has ended.**
> Please migrate to the MQTT version going forward.
> → https://github.com/noritama-lab/esp32io-mqtt-firmware

## Overview

**ESP32-S3 Remote I/O Firmware** is a **general-purpose JSON-based remote I/O firmware** for ESP32-S3.  
It exposes a unified JSON API over USB Serial and HTTP, enabling **any environment**—Python, Excel VBA, Node-RED, Power Automate, curl, JavaScript, C#, Go, and more—to control the device simply by sending JSON commands.

> **Any environment capable of sending JSON can control the device.**

Related Project:  
- https://github.com/Noritama-Lab/esp32io-api

---

## Features

- 6 Digital Inputs  
- 6 Digital Outputs  
- 2 ADC Inputs  
- 2 PWM Outputs  
- USB CDC JSON API  
- HTTP JSON API  
- Wi-Fi STA & AP Modes  
- mDNS Support  
- NeoPixel Status LED  
- I2C Expansion  
- Built-in support for BME280 / MPU6050 / VL53L1X / SSD1306  
- Factory Reset via BOOT button  
- Persistent settings via NVS  

---

## Hardware Configuration & Pin Mapping

| Function | pin_id | GPIO |
|---|---|---|
| Digital Input | 0–5 | 4, 5, 6, 7, 8, 9 |
| Digital Output | 0–5 | 10, 11, 12, 13, 14, 15 |
| ADC Input | 0–1 | 1, 2 |
| PWM Output | 0–1 | 36, 37 |
| Status LED | - | 38 |
| I2C (SDA, SCL) | - | 40, 41 |
| Reset Button | - | 0 (BOOT) |

---

## Required Libraries (Arduino Library Manager)

- ArduinoJson (v7)
- Adafruit BME280 / MPU6050 / SSD1306 / VL53L1X libraries

---

## Setup

1. On boot, the device starts a WiFi AP named `ESP32_S3_IO_<MAC suffix>` (password: `esp32setup`).
2. Connect to the AP and open `http://192.168.4.1/` in a browser.
3. Enter your WiFi SSID/password and save; the device restarts and connects to WiFi.
4. Once connected, the config page and sensor monitor are available at `http://<device_ip>/` or `http://ESP32_S3_IO_XXXXXX.local/`.
5. USB Serial (115200bps) also works out of the box for sending/receiving JSON commands without any WiFi setup.

---

## API Reference (USB / HTTP Shared)

All commands follow this format:

```json
{"cmd":"command_name", ...}
```

See **[COMMAND_REFERENCE.md](COMMAND_REFERENCE.md)** for the full list of commands (`read_di` / `set_do` / `get_io_state` / `read_adc` / `set_pwm` / `get_pwm_config` / `set_pwm_config` / `i2c_scan` / `i2c_write` / `i2c_read` / `get_sensors` / `set_oled` / `set_rgb` / `led_off` / `set_led_mode` / `get_led_state` / `get_status` / `ping` / `help`) with parameters and response formats.

---

## Use from Any Environment

### Python
```python
requests.post("http://device.local/api", json={"cmd":"ping"})
```

### curl
```bash
curl -X POST http://device.local/api -d '{"cmd":"ping"}'
```

### JavaScript
```js
fetch("/api", {method:"POST", body:JSON.stringify({cmd:"ping"})})
```

### Node-RED / Power Automate / C# / Go / etc.
> **Any environment capable of sending JSON can control the device.**

---

## License

MIT License  
Copyright (c) 2026 Noritama-Lab
