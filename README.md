# ESP32-S3 Remote I/O Device  
プロフェッショナル用途にも耐える、扱いやすいリモート I/O デバイス用ファームウェア  
Professional‑grade yet easy‑to‑use firmware for remote I/O devices.

[日本語](#日本語版) | [English](#english-version)

---

# 日本語版

# ESP32-S3 リモート I/O デバイス

ESP32-S3 をベースにした、**デジタル入出力・アナログ入力（ADC）・PWM 出力を JSON で制御できるリモート I/O デバイス**です。  
USB シリアルと HTTP API のどちらからでも同じ JSON コマンドで操作できるため、スクリプト・GUI・Excel・Node-RED など、好きな環境から扱えます。

本ファームウェアは以下の 2 種類を提供します：

- **USB 専用版（ESP32_S3_IO_DEVICE）**  
  → 最もシンプルで高速。開発・組込み用途に最適。

- **Wi‑Fi + USB 版（ESP32_S3_IO_DEVICE_NET）**  
  → HTTP API / mDNS / Wi‑Fi 設定ポータル付き。リモート制御向け。

---

## 主な特徴（共通）

- **統一された JSON API**  
  DIO / ADC / PWM を同じ形式で扱えるため、クライアント実装がシンプル。

- **USB と HTTP のデュアルインターフェース（NET 版）**  
  開発中は USB、運用時は HTTP と、状況に応じて柔軟に切り替え可能。

- **堅牢な設計**  
  - BOOT ボタンでファクトリリセット  
  - NVS による設定永続化  
  - ノンブロッキング処理で高い応答性

---

# 2つのファームウェアの違い

## USB 専用版（ESP32_S3_IO_DEVICE）

| 項目 | 内容 |
|---|---|
| 通信方式 | USB CDC（シリアル） |
| API | JSON（1行ごと） |
| Wi‑Fi | なし |
| HTTP API | なし |
| mDNS | なし |
| LED | シンプルな状態表示のみ |
| 初期設定 | USB を挿すだけで即利用可能 |
| 用途 | 開発、組込み、PC 直結制御、Excel/Node‑RED/GUI 連携 |

**特徴：**  
- 最も軽量で高速  
- ネットワーク不要  
- 電源を入れた瞬間に使える  
- USB 給電だけで動作

---

## Wi‑Fi + USB 版（ESP32_S3_IO_DEVICE_NET）

| 項目 | 内容 |
|---|---|
| 通信方式 | USB CDC + HTTP API |
| API | JSON（USB / HTTP 共通） |
| Wi‑Fi | STA + AP モード |
| HTTP API | `/api?cmd=...` |
| mDNS | `http://ESP32_S3_IO_xxxxxx.local` |
| LED | NeoPixel による状態エフェクト |
| 初期設定 | AP モードで Wi‑Fi 設定 |
| 用途 | リモート制御、IoT、スマホ操作、LAN 内制御 |

**特徴：**  
- スマホから設定可能  
- ネットワーク越しに制御できる  
- mDNS による簡単アクセス  
- LED エフェクトで状態がわかりやすい

---

# ハードウェア設定（共通）

| 機能 | ピン (ESP32-S3) |
|---|---|
| デジタル入力 | 4, 5, 6, 7, 8, 9 |
| デジタル出力 | 10, 11, 12, 13, 14, 15 |
| アナログ入力 (ADC) | 1, 2 |
| PWM 出力 | 38, 39 |
| ステータス LED (NeoPixel) | 48 |
| リセットボタン | 0 (BOOT) |

---

# はじめに

# USB 専用版の使い方（ESP32_S3_IO_DEVICE）

### 1. 書き込み  
Arduino IDE または PlatformIO で書き込み。

### 2. 接続  
USB ケーブルを挿すだけで準備完了。

### 3. シリアル通信  
115200bps / 改行区切りの JSON を送信。

例：

```json
{"cmd":"set_do","pin_id":0,"value":1}
```

---

# Wi‑Fi + USB 版の使い方（ESP32_S3_IO_DEVICE_NET）

### 1. 初回起動（AP モード）  
- SSID：`ESP32_S3_IO_XXXXXX`  
  - **XXXXXX は MAC アドレス下位 3 バイトから生成**  
- `http://192.168.4.1` を開く  
- Wi‑Fi 設定を保存

### 2. 通常アクセス  
- `http://ESP32_S3_IO_XXXXXX.local`  
- または DHCP で割り当てられた IP

---

# API リファレンス（USB / HTTP 共通）

すべてのコマンドは以下形式：

```json
{"cmd": "コマンド名", ...}
```

## Digital IO

| コマンド | 説明 | 例 |
|---|---|---|
| `read_di` | デジタル入力を読む | `{"cmd":"read_di","pin_id":0}` |
| `set_do` | デジタル出力を設定 | `{"cmd":"set_do","pin_id":0,"value":1}` |

## ADC

| コマンド | 説明 | 例 |
|---|---|---|
| `read_adc` | アナログ入力を読む | `{"cmd":"read_adc","pin_id":0}` |

## PWM

| コマンド | 説明 | 例 |
|---|---|---|
| `set_pwm` | デューティ設定 | `{"cmd":"set_pwm","pin_id":0,"duty":128}` |
| `set_pwm_config` | 周波数/分解能設定 | `{"cmd":"set_pwm_config","freq":5000,"res":8}` |
| `get_pwm_config` | 現在の設定取得 | `{"cmd":"get_pwm_config"}` |

## LED（NET 版のみ）

| コマンド | 説明 | 例 |
|---|---|---|
| `set_rgb` | 手動 RGB 制御 | `{"cmd":"set_rgb","r":255,"g":0,"b":0,"brightness":10}` |
| `set_led_mode` | モード切替 | `{"cmd":"set_led_mode","mode":"status"}` |
| `get_led_state` | LED 状態取得 | `{"cmd":"get_led_state"}` |

## システム

| コマンド | 説明 | 例 |
|---|---|---|
| `get_io_state` | 全 I/O 状態取得 | `{"cmd":"get_io_state"}` |
| `get_status` | システム情報 | `{"cmd":"get_status"}` |
| `ping` | 生存確認 | `{"cmd":"ping"}` |
| `help` | コマンド一覧 | `{"cmd":"help"}` |

---

# Python からの利用例

## USB 専用版

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

## Wi‑Fi 版

```python
import requests

BASE_URL = "http://ESP32_S3_IO_XXXXXX.local/api"

def send(cmd):
    resp = requests.get(BASE_URL, params=cmd)
    print("Response:", resp.json())

send({"cmd": "read_adc", "pin_id": 0})
```

---

# プロジェクト構成

- `Config.h` — ピン定義・定数  
- `HardwareManager` — IO 制御・LED エフェクト  
- `AppNetworkManager` — WiFi / mDNS / NVS（NET 版）  
- `WebHandler` — Web サーバ（NET 版）  
- `CommandHandler` — JSON コマンド処理（USB / HTTP 共通）

---

# ライセンス

MIT License  
詳細は LICENSE を参照してください。

---

# English Version

# ESP32-S3 Remote I/O Device
This project provides firmware that turns an ESP32‑S3 into a **remote I/O device controllable via JSON commands**, supporting **Digital IO, ADC, and PWM**.  
Both USB Serial and HTTP API use the same JSON command structure, making it easy to integrate with scripts, GUIs, Excel, Node‑RED, and more.

Two firmware variants are available:

- **USB‑Only Version (ESP32_S3_IO_DEVICE)**  
  → The simplest and fastest option. Ideal for development and embedded use.

- **Wi‑Fi + USB Version (ESP32_S3_IO_DEVICE_NET)**  
  → Includes HTTP API, mDNS, and a Wi‑Fi setup portal. Designed for remote control.

---

## Key Features (Common)

- **Unified JSON API**  
  Digital IO / ADC / PWM all share the same command structure, keeping client code simple.

- **Dual Interface (NET version)**  
  Use USB during development and HTTP during deployment — switch freely as needed.

- **Robust Architecture**  
  - Factory reset via BOOT button  
  - Persistent settings stored in NVS  
  - Non‑blocking processing for high responsiveness

---

# Differences Between the Two Firmware Variants

## USB‑Only Version (ESP32_S3_IO_DEVICE)

| Item | Description |
|---|---|
| Communication | USB CDC (Serial) |
| API | JSON (one command per line) |
| Wi‑Fi | Not supported |
| HTTP API | Not supported |
| mDNS | Not supported |
| LED | Simple status indication only |
| Setup | Plug in USB and start using immediately |
| Use Cases | Development, embedded systems, PC‑direct control, Excel/Node‑RED/GUI integration |

**Highlights:**  
- Fastest and lightest  
- No network required  
- Works instantly after power‑on  
- Operates with USB power only

---

## Wi‑Fi + USB Version (ESP32_S3_IO_DEVICE_NET)

| Item | Description |
|---|---|
| Communication | USB CDC + HTTP API |
| API | JSON (USB / HTTP shared) |
| Wi‑Fi | STA + AP mode |
| HTTP API | `/api?cmd=...` |
| mDNS | `http://ESP32_S3_IO_xxxxxx.local` |
| LED | NeoPixel status effects |
| Setup | Wi‑Fi configuration via AP mode |
| Use Cases | Remote control, IoT, smartphone access, LAN‑based automation |

**Highlights:**  
- Wi‑Fi setup via smartphone  
- Remote control over the network  
- Easy access via mDNS  
- Clear visual feedback through LED effects

---

# Default Hardware Configuration (Common)

| Function | Pin (ESP32‑S3) |
|---|---|
| Digital Input | 4, 5, 6, 7, 8, 9 |
| Digital Output | 10, 11, 12, 13, 14, 15 |
| ADC Input | 1, 2 |
| PWM Output | 38, 39 |
| Status LED (NeoPixel) | 48 |
| Reset Button | 0 (BOOT) |

---

# Getting Started

# USB‑Only Version (ESP32_S3_IO_DEVICE)

### 1. Flash the Firmware  
Upload using Arduino IDE or PlatformIO.

### 2. Connect  
Simply plug in the USB cable — no configuration required.

### 3. Serial Communication  
115200 bps, JSON commands separated by newline.

Example:

```json
{"cmd":"set_do","pin_id":0,"value":1}
```

---

# Wi‑Fi + USB Version (ESP32_S3_IO_DEVICE_NET)

### 1. First Boot (AP Mode)  
- SSID: `ESP32_S3_IO_XXXXXX`  
  - **XXXXXX is generated from the lower 3 bytes of the MAC address**
- Connect to the AP  
- Open `http://192.168.4.1`  
- Save Wi‑Fi settings

### 2. Normal Access  
- `http://ESP32_S3_IO_XXXXXX.local`  
- Or the DHCP‑assigned IP address

---

# API Reference (USB / HTTP Shared)

All commands follow this structure:

```json
{"cmd": "command_name", ...}
```

## Digital IO

| Command | Description | Example |
|---|---|---|
| `read_di` | Read digital input | `{"cmd":"read_di","pin_id":0}` |
| `set_do` | Set digital output | `{"cmd":"set_do","pin_id":0,"value":1}` |

## ADC

| Command | Description | Example |
|---|---|---|
| `read_adc` | Read analog input | `{"cmd":"read_adc","pin_id":0}` |

## PWM

| Command | Description | Example |
|---|---|---|
| `set_pwm` | Set duty | `{"cmd":"set_pwm","pin_id":0,"duty":128}` |
| `set_pwm_config` | Set frequency/resolution | `{"cmd":"set_pwm_config","freq":5000,"res":8}` |
| `get_pwm_config` | Get current config | `{"cmd":"get_pwm_config"}` |

## LED (NET Version Only)

| Command | Description | Example |
|---|---|---|
| `set_rgb` | Manual RGB control | `{"cmd":"set_rgb","r":255,"g":0,"b":0,"brightness":10}` |
| `set_led_mode` | Change LED mode | `{"cmd":"set_led_mode","mode":"status"}` |
| `get_led_state` | Get LED state | `{"cmd":"get_led_state"}` |

## System

| Command | Description | Example |
|---|---|---|
| `get_io_state` | Get all IO states | `{"cmd":"get_io_state"}` |
| `get_status` | System info | `{"cmd":"get_status"}` |
| `ping` | Health check | `{"cmd":"ping"}` |
| `help` | List commands | `{"cmd":"help"}` |

---

# Python Usage Examples

## USB‑Only Version

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

## Wi‑Fi Version

```python
import requests

BASE_URL = "http://ESP32_S3_IO_XXXXXX.local/api"

def send(cmd):
    resp = requests.get(BASE_URL, params=cmd)
    print("Response:", resp.json())

send({"cmd": "read_adc", "pin_id": 0})
```

---

# Project Structure

- `Config.h` — Pin definitions & constants  
- `HardwareManager` — IO control & LED effects  
- `AppNetworkManager` — Wi‑Fi / mDNS / NVS (NET version)  
- `WebHandler` — Web server (NET version)  
- `CommandHandler` — Unified JSON command processor (USB / HTTP shared)

---

# License

MIT License  
See LICENSE for details.

