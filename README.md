# ESP32-S3 Remote I/O Firmware
JSON-based USB and HTTP remote I/O firmware for ESP32-S3.

[日本語](#日本語版) | [English](#english-version)

---

# 日本語版

## 概要

**ESP32-S3 Remote I/O Firmware** は、Pythonライブラリ **ESP32IO** 向けに設計されたファームウェアです。

USBシリアルおよびHTTP経由で統一されたJSON APIを提供し、ESP32IOやその他のソフトウェアからデジタルI/O、ADC、PWM、各種センサー、ディスプレイをシームレスに制御できます。

関連プロジェクト:

- [ESP32IO Python Library](https://github.com/your-username/ESP32IO)

---

## 主な機能 (Features)

- 6x デジタル入力
- 6x デジタル出力
- 2x アナログ入力 (ADC)
- 2x PWM 出力
- USB CDC JSON API
- HTTP JSON API
- Wi-Fi STA & AP モード
- mDNS サポート
- NeoPixel 状態表示 LED (GPIO48)
- I2C 拡張サポート
- BME280 / MPU6050 / VL53L0X / SSD1306 対応
- BOOTボタン長押しによるファクトリーリセット
- NVSによる設定保存

| 項目 | 内容 |
|---|---|
| 通信方式 | USB CDC (Serial) + HTTP API |
| Wi-Fi | STA + AP モード |
| HTTP API | `POST /api` (JSON Body) |
| mDNS | `http://ESP32_S3_IO_XXXXXX.local` |
| LED | NeoPixel 状態表示 (GPIO48) |
| 初期設定 | APモードによるWi-Fi設定ポータル |
| 用途 | 開発、組込み、リモート制御、IoT、LAN制御 |

---

## ハードウェア設定 & ピンマッピング

| 機能タイプ | pin_id (API) | GPIO ピン番号 |
|---|---|---|
| Digital Input | 0, 1, 2, 3, 4, 5 | 4, 5, 6, 7, 8, 9 |
| Digital Output | 0, 1, 2, 3, 4, 5 | 10, 11, 12, 13, 14, 15 |
| ADC Input | 0, 1 | 1, 2 |
| PWM Output | 0, 1 | 38, 39 |
| Status LED | - | 48 |
| I2C (SDA, SCL) | - | 40, 41 |
| Reset Button | - | 0 (BOOT) |

---

## クイックスタート

### 1. ファームウェア書き込み

Arduino IDE または PlatformIO を使用してファームウェアを書き込みます。

---

### USBシリアル / HTTP API

すべての制御は JSON で行います。

HTTPでは `POST /api` に JSON Body を送信します。

#### Request (制御例)

```json
{"cmd":"set_do","pin_id":0,"value":1}
```

#### Response

```json
{"status":"ok"}
```

---

### Wi-Fi / HTTP 初回設定

- SSID: `ESP32_S3_IO_XXXXXX`
- XXXXXX は MACアドレス下位3バイトから生成
- `http://192.168.4.1` を開く
- Wi-Fi設定を保存

---

### 通常アクセス

- `http://ESP32_S3_IO_XXXXXX.local`
- または DHCPで割り当てられたIPアドレス

---

## API リファレンス（USB / HTTP 共通）

すべてのコマンドは以下形式です。

```json
{"cmd":"command_name", ...}
```

### Digital IO

| コマンド | 説明 | 例 |
|---|---|---|
| `read_di` | デジタル入力を読む | `{"cmd":"read_di","pin_id":0}` |
| `set_do` | デジタル出力を設定 | `{"cmd":"set_do","pin_id":0,"value":1}` |

### ADC

| コマンド | 説明 | 例 |
|---|---|---|
| `read_adc` | アナログ入力を読む | `{"cmd":"read_adc","pin_id":0}` |

### PWM

| コマンド | 説明 | 例 |
|---|---|---|
| `set_pwm` | デューティ設定 | `{"cmd":"set_pwm","pin_id":0,"duty":128}` |
| `set_pwm_config` | 周波数・分解能設定 | `{"cmd":"set_pwm_config","freq":5000,"res":8}` |
| `get_pwm_config` | 現在の設定取得 | `{"cmd":"get_pwm_config"}` |

### I2C / Sensors / Display

| コマンド | 説明 | 例 |
|---|---|---|
| `i2c_scan` | I2Cデバイススキャン | `{"cmd":"i2c_scan"}` |
| `i2c_write` | データ書き込み | `{"cmd":"i2c_write","addr":60,"data":[0,174]}` |
| `i2c_read` | データ読み取り | `{"cmd":"i2c_read","addr":60,"len":2}` |
| `get_sensors` | センサー値取得 | `{"cmd":"get_sensors"}` |
| `set_oled` | OLED文字表示 | `{"cmd":"set_oled","text":"Hello","x":0,"y":0}` |

### LED

| コマンド | 説明 | 例 |
|---|---|---|
| `set_rgb` | RGB制御 | `{"cmd":"set_rgb","r":255,"g":0,"b":0,"brightness":10}` |
| `led_off` | LED消灯 | `{"cmd":"led_off"}` |
| `set_led_mode` | LEDモード変更 | `{"cmd":"set_led_mode","mode":"status"}` |
| `get_led_state` | LED状態取得 | `{"cmd":"get_led_state"}` |

### System

| コマンド | 説明 | 例 |
|---|---|---|
| `get_io_state` | 全I/O状態取得 | `{"cmd":"get_io_state"}` |
| `get_status` | システム情報取得 | `{"cmd":"get_status"}` |
| `ping` | 生存確認 | `{"cmd":"ping"}` |
| `help` | コマンド一覧 | `{"cmd":"help"}` |

---

## Python からの利用例

### USB Serial (CDC)

```python
import serial
import json
import time

ser = serial.Serial("COM3", 115200, timeout=1)

def send(cmd):
    ser.write((json.dumps(cmd) + "\n").encode())
    time.sleep(0.05)
    print(ser.readline().decode().strip())

send({"cmd":"ping"})
send({"cmd":"set_do","pin_id":0,"value":1})
```

### HTTP API (Wi-Fi)

```python
import requests

BASE_URL = "http://ESP32_S3_IO_XXXXXX.local/api"

def send(cmd):
    resp = requests.post(BASE_URL, json=cmd)
    print(resp.json())

send({"cmd":"read_adc","pin_id":0})
```

---

## プロジェクト構成

- `Config.h` — ピン定義・定数
- `HardwareManager` — IO制御・LEDエフェクト
- `AppNetworkManager` — Wi-Fi / mDNS / NVS
- `WebHandler` — Webサーバ
- `CommandHandler` — USB / HTTP共通JSONコマンド処理

---

## ライセンス

MIT License

Copyright (c) 2026 Noritama-Lab

詳細は LICENSE.md を参照してください。

---

# English Version

## Overview

**ESP32-S3 Remote I/O Firmware** is the official firmware for the ESP32IO Python library.

It provides a unified JSON API over USB Serial and HTTP, allowing ESP32IO and other software to control Digital I/O, ADC, PWM, sensors, and displays.

Related Project:

- ESP32IO Python Library

---

## Features

- 6x Digital Inputs
- 6x Digital Outputs
- 2x ADC Inputs
- 2x PWM Outputs
- USB CDC JSON API
- HTTP JSON API
- Wi-Fi STA & AP Modes
- mDNS Support
- NeoPixel Status LED (GPIO48)
- I2C Expansion Support
- Built-in support for BME280, MPU6050, VL53L0X and SSD1306
- Factory Reset via BOOT button
- Persistent settings using NVS

| Item | Description |
|---|---|
| Communication | USB CDC (Serial) + HTTP API |
| Wi-Fi | STA + AP Mode |
| HTTP API | `POST /api` (JSON Body) |
| mDNS | `http://ESP32_S3_IO_XXXXXX.local` |
| LED | NeoPixel Status Indicator (GPIO48) |
| Initial Setup | Wi-Fi Configuration Portal via AP Mode |
| Use Cases | Development, Embedded Systems, Remote Control, IoT, LAN Automation |

---

## Hardware Configuration & Pin Mapping

| Function Type | pin_id (API) | GPIO Pin |
|---|---|---|
| Digital Input | 0, 1, 2, 3, 4, 5 | 4, 5, 6, 7, 8, 9 |
| Digital Output | 0, 1, 2, 3, 4, 5 | 10, 11, 12, 13, 14, 15 |
| ADC Input | 0, 1 | 1, 2 |
| PWM Output | 0, 1 | 38, 39 |
| Status LED | - | 48 |
| I2C (SDA, SCL) | - | 40, 41 |
| Reset Button | - | 0 (BOOT) |

---

## Quick Start

### 1. Flash the Firmware

Upload the firmware using Arduino IDE or PlatformIO.

---

### USB Serial / HTTP API

All device control is performed using JSON commands.

For HTTP, send JSON requests to `POST /api`.

#### Request

```json
{"cmd":"set_do","pin_id":0,"value":1}
```

#### Response

```json
{"status":"ok"}
```

---

### Wi-Fi / HTTP First-Time Setup

- SSID: `ESP32_S3_IO_XXXXXX`
- XXXXXX is generated from the lower 3 bytes of the MAC address
- Open `http://192.168.4.1`
- Save Wi-Fi settings

---

### Normal Access

- `http://ESP32_S3_IO_XXXXXX.local`
- Or the DHCP-assigned IP address

---

## API Reference (USB / HTTP Shared)

All commands use the following format:

```json
{"cmd":"command_name", ...}
```

### Digital IO

| Command | Description | Example |
|---|---|---|
| `read_di` | Read Digital Input | `{"cmd":"read_di","pin_id":0}` |
| `set_do` | Set Digital Output | `{"cmd":"set_do","pin_id":0,"value":1}` |

### ADC

| Command | Description | Example |
|---|---|---|
| `read_adc` | Read Analog Input | `{"cmd":"read_adc","pin_id":0}` |

### PWM

| Command | Description | Example |
|---|---|---|
| `set_pwm` | Set Duty Cycle | `{"cmd":"set_pwm","pin_id":0,"duty":128}` |
| `set_pwm_config` | Configure Frequency / Resolution | `{"cmd":"set_pwm_config","freq":5000,"res":8}` |
| `get_pwm_config` | Get Current Configuration | `{"cmd":"get_pwm_config"}` |

### I2C / Sensors / Display

| Command | Description | Example |
|---|---|---|
| `i2c_scan` | Scan I2C Bus | `{"cmd":"i2c_scan"}` |
| `i2c_write` | Write Raw Data | `{"cmd":"i2c_write","addr":60,"data":[0,174]}` |
| `i2c_read` | Read Raw Data | `{"cmd":"i2c_read","addr":60,"len":2}` |
| `get_sensors` | Read All Sensors | `{"cmd":"get_sensors"}` |
| `set_oled` | Display Text on OLED | `{"cmd":"set_oled","text":"Hello","x":0,"y":0}` |

### LED

| Command | Description | Example |
|---|---|---|
| `set_rgb` | Manual RGB Control | `{"cmd":"set_rgb","r":255,"g":0,"b":0,"brightness":10}` |
| `led_off` | Turn Off LED | `{"cmd":"led_off"}` |
| `set_led_mode` | Change LED Mode | `{"cmd":"set_led_mode","mode":"status"}` |
| `get_led_state` | Get LED State | `{"cmd":"get_led_state"}` |

### System

| Command | Description | Example |
|---|---|---|
| `get_io_state` | Get All I/O States | `{"cmd":"get_io_state"}` |
| `get_status` | Get System Information | `{"cmd":"get_status"}` |
| `ping` | Health Check | `{"cmd":"ping"}` |
| `help` | List Available Commands | `{"cmd":"help"}` |

---

## Python Usage Examples

### USB Serial (CDC)

```python
import serial
import json
import time

ser = serial.Serial("COM3", 115200, timeout=1)

def send(cmd):
    ser.write((json.dumps(cmd) + "\n").encode())
    time.sleep(0.05)
    print(ser.readline().decode().strip())

send({"cmd":"ping"})
send({"cmd":"set_do","pin_id":0,"value":1})
```

### HTTP API (Wi-Fi)

```python
import requests

BASE_URL = "http://ESP32_S3_IO_XXXXXX.local/api"

def send(cmd):
    resp = requests.post(BASE_URL, json=cmd)
    print(resp.json())

send({"cmd":"read_adc","pin_id":0})
```

---

## Project Structure

- `Config.h` — Pin Definitions & Constants
- `HardwareManager` — IO Control & LED Effects
- `AppNetworkManager` — Wi-Fi / mDNS / NVS
- `WebHandler` — Web Server
- `CommandHandler` — Unified JSON Command Processor (USB / HTTP Shared)

---

## License

MIT License

Copyright (c) 2026 Noritama-Lab

See LICENSE.md for details.