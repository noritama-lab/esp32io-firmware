# ESP32‑S3 Remote I/O Device  
プロフェッショナル用途にも耐える、扱いやすいリモート I/O デバイス用ファームウェア  
Professional‑grade yet easy‑to‑use firmware for remote I/O devices.

[日本語](#日本語版) | [English](#english-version)

---

# 日本語版

## 概要
ESP32‑S3 をベースにした、**デジタル入出力・アナログ入力（ADC）・PWM 出力を JSON で制御できるリモート I/O デバイス**用ファームウェアです。  
USB シリアル（CDC）と HTTP API の両インターフェースを備え、スクリプト、GUI、Excel、Node‑RED など、あらゆる環境からシームレスに操作できます。

---

## 主な特徴

- **統一された JSON API**  
  DIO / ADC / PWM を同じ形式で扱えるため、クライアント実装がシンプル。

- **USB と HTTP のデュアルインターフェース**  
  開発中は USB、運用時は HTTP と柔軟に切り替え可能。

- **堅牢な設計**  
  - BOOT ボタンでファクトリリセット  
  - NVS による Wi‑Fi 設定の永続化  
  - ノンブロッキング処理で高い応答性

- **視覚的な状態表示**  
  NeoPixel LED による接続状態やエラーのエフェクト表示

| 項目 | 内容 |
|---|---|
| 通信方式 | USB CDC (Serial) + HTTP API |
| Wi‑Fi | STA + AP モード |
| HTTP API | `/api?cmd=...` |
| mDNS | `http://ESP32_S3_IO_XXXXXX.local` |
| LED | NeoPixel 状態表示 (GPIO48) |
| 初期設定 | AP モードによる Wi‑Fi 設定ポータル |
| 用途 | 開発、組込み、リモート制御、IoT、LAN 制御 |

---

## ハードウェア設定

| 機能 | ピン (ESP32‑S3) |
|---|---|
| デジタル入力 | 4, 5, 6, 7, 8, 9 |
| デジタル出力 | 10, 11, 12, 13, 14, 15 |
| アナログ入力 (ADC) | 1, 2 |
| PWM 出力 | 38, 39 |
| ステータス LED (NeoPixel) | 48 |
| リセットボタン | 0 (BOOT) |

---

## クイックスタート

### 1. 書き込み
Arduino IDE または PlatformIO でファームウェアを書き込みます。

---

### USB シリアルでの利用
USB を PC に接続し、シリアルモニタ（115200bps）から JSON を送信します。

```json
{"cmd":"ping"}
```

---

### Wi‑Fi / HTTP での利用（初回設定）
- SSID：`ESP32_S3_IO_XXXXXX`  
  - **XXXXXX は MAC アドレス下位 3 バイトから生成**
- `http://192.168.4.1` を開く  
- Wi‑Fi 設定を保存

---

### 通常アクセス
- `http://ESP32_S3_IO_XXXXXX.local`  
- または DHCP で割り当てられた IP

---

## API リファレンス（USB / HTTP 共通）

すべてのコマンドは以下形式：

```json
{"cmd": "コマンド名", ...}
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
| `set_pwm_config` | 周波数/分解能設定 | `{"cmd":"set_pwm_config","freq":5000,"res":8}` |
| `get_pwm_config` | 現在の設定取得 | `{"cmd":"get_pwm_config"}` |

### LED

| コマンド | 説明 | 例 |
|---|---|---|
| `set_rgb` | 手動 RGB 制御 | `{"cmd":"set_rgb","r":255,"g":0,"b":0,"brightness":10}` |
| `set_led_mode` | モード切替 | `{"cmd":"set_led_mode","mode":"status"}` |
| `get_led_state` | LED 状態取得 | `{"cmd":"get_led_state"}` |

### システム

| コマンド | 説明 | 例 |
|---|---|---|
| `get_io_state` | 全 I/O 状態取得 | `{"cmd":"get_io_state"}` |
| `get_status` | システム情報 | `{"cmd":"get_status"}` |
| `ping` | 生存確認 | `{"cmd":"ping"}` |
| `help` | コマンド一覧 | `{"cmd":"help"}` |

---

## Python からの利用例

### USB シリアル（CDC）

```python
import serial, json, time

ser = serial.Serial("COMX", 115200, timeout=1)

def send(cmd):
    ser.write((json.dumps(cmd) + "\n").encode())
    time.sleep(0.05)
    print("Response:", ser.readline().decode().strip())

send({"cmd": "ping"})
send({"cmd": "set_do", "pin_id": 0, "value": 1})
```

### HTTP API（Wi‑Fi 経由）

```python
import requests

BASE_URL = "http://ESP32_S3_IO_XXXXXX.local/api"

def send(cmd):
    resp = requests.get(BASE_URL, params=cmd)
    print("Response:", resp.json())

send({"cmd": "read_adc", "pin_id": 0})
```

---

## プロジェクト構成

- `Config.h` — ピン定義・定数  
- `HardwareManager` — IO 制御・LED エフェクト  
- `AppNetworkManager` — Wi‑Fi / mDNS / NVS  
- `WebHandler` — Web サーバ  
- `CommandHandler` — JSON コマンド処理（USB / HTTP 共通）

---

## ライセンス
MIT License  
Copyright (c) 2026 Noritama‑Lab  
詳細は LICENSE.md を参照してください。

---

# English Version

## Overview
This firmware turns an ESP32‑S3 into a **remote I/O device controllable via JSON commands**, supporting **Digital IO, ADC, and PWM**.  
It provides both **USB Serial (CDC)** and **HTTP API**, enabling seamless integration with scripts, GUIs, Excel, Node‑RED, and more.

---

## Key Features

- **Unified JSON API**  
  Digital IO / ADC / PWM all share the same command structure.

- **Dual Interface: USB + HTTP**  
  USB for development, HTTP for deployment — switch freely as needed.

- **Robust Architecture**  
  - Factory reset via BOOT button  
  - Persistent Wi‑Fi settings stored in NVS  
  - Non‑blocking processing for high responsiveness

- **Visual Status Indication**  
  NeoPixel LED shows connection status and error effects.

| Item | Description |
|---|---|
| Communication | USB CDC (Serial) + HTTP API |
| Wi‑Fi | STA + AP mode |
| HTTP API | `/api?cmd=...` |
| mDNS | `http://ESP32_S3_IO_XXXXXX.local` |
| LED | NeoPixel status indicator (GPIO48) |
| Initial Setup | Wi‑Fi configuration portal via AP mode |
| Use Cases | Development, embedded systems, remote control, IoT, LAN automation |

---

## Hardware Configuration

| Function | Pin (ESP32‑S3) |
|---|---|
| Digital Input | 4, 5, 6, 7, 8, 9 |
| Digital Output | 10, 11, 12, 13, 14, 15 |
| ADC Input | 1, 2 |
| PWM Output | 38, 39 |
| Status LED (NeoPixel) | 48 |
| Reset Button | 0 (BOOT) |

---

## Quick Start

### 1. Flash the Firmware
Upload using Arduino IDE or PlatformIO.

---

### USB Serial Usage
Connect via USB and send JSON commands at **115200 bps**.

```json
{"cmd":"ping"}
```

---

### Wi‑Fi / HTTP Usage (First‑time Setup)
- SSID: `ESP32_S3_IO_XXXXXX`  
  - **XXXXXX is generated from the lower 3 bytes of the MAC address**
- Open `http://192.168.4.1`
- Save Wi‑Fi settings

---

### Normal Access
- `http://ESP32_S3_IO_XXXXXX.local`  
- Or the DHCP‑assigned IP address

---

## API Reference (USB / HTTP Shared)

All commands follow this structure:

```json
{"cmd": "command_name", ...}
```

### Digital IO

| Command | Description | Example |
|---|---|---|
| `read_di` | Read digital input | `{"cmd":"read_di","pin_id":0}` |
| `set_do` | Set digital output | `{"cmd":"set_do","pin_id":0,"value":1}` |

### ADC

| Command | Description | Example |
|---|---|---|
| `read_adc` | Read analog input | `{"cmd":"read_adc","pin_id":0}` |

### PWM

| Command | Description | Example |
|---|---|---|
| `set_pwm` | Set duty | `{"cmd":"set_pwm","pin_id":0,"duty":128}` |
| `set_pwm_config` | Set frequency/resolution | `{"cmd":"set_pwm_config","freq":5000,"res":8}` |
| `get_pwm_config` | Get current config | `{"cmd":"get_pwm_config"}` |

### LED

| Command | Description | Example |
|---|---|---|
| `set_rgb` | Manual RGB control | `{"cmd":"set_rgb","r":255,"g":0,"b":0,"brightness":10}` |
| `set_led_mode` | Change LED mode | `{"cmd":"set_led_mode","mode":"status"}` |
| `get_led_state` | Get LED state | `{"cmd":"get_led_state"}` |

### System

| Command | Description | Example |
|---|---|---|
| `get_io_state` | Get all IO states | `{"cmd":"get_io_state"}` |
| `get_status` | System info | `{"cmd":"get_status"}` |
| `ping` | Health check | `{"cmd":"ping"}` |
| `help` | List commands | `{"cmd":"help"}` |

---

## Python Usage Examples

### USB Serial (CDC)

```python
import serial, json, time

ser = serial.Serial("COMX", 115200, timeout=1)

def send(cmd):
    ser.write((json.dumps(cmd) + "\n").encode())
    time.sleep(0.05)
    print("Response:", ser.readline().decode().strip())

send({"cmd": "ping"})
send({"cmd": "set_do", "pin_id": 0, "value": 1})
```

### HTTP API over Wi‑Fi

```python
import requests

BASE_URL = "http://ESP32_S3_IO_XXXXXX.local/api"

def send(cmd):
    resp = requests.get(BASE_URL, params=cmd)
    print("Response:", resp.json())

send({"cmd": "read_adc", "pin_id": 0})
```

---

## Project Structure

- `Config.h` — Pin definitions & constants  
- `HardwareManager` — IO control & LED effects  
- `AppNetworkManager` — Wi‑Fi / mDNS / NVS  
- `WebHandler` — Web server  
- `CommandHandler` — Unified JSON command processor (USB / HTTP shared)

---

## License
MIT License  
Copyright (c) 2026 Noritama‑Lab

