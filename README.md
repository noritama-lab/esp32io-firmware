# ESP32IO Firmware  
日本語 / English

---

## 🇯🇵 ESP32IO Firmware

ESP32IO Firmware は、ESP32‑S3 を USB 経由で I/O デバイスとして扱えるようにするファームウェアです。  
Python API（esp32io）と組み合わせることで、USB シリアル経由で I/O を安全かつ直感的に制御できます。

### 特徴

- USB CDC（シリアル）で PC と通信  
- JSON ベースのコマンドプロトコル  
- ADC / PWM / DIO（入力・出力）をサポート  
- 統一されたレスポンス形式とエラー処理  
- **PWM の周波数・分解能を設定可能（NVS に保存）**  
- Python API と 1 対 1 で対応するコマンドセット  
- USB 接続だけで動作、追加設定不要  

---

## リポジトリ構成

```
esp32io-firmware/
├── ESP32_S3_IO_DEVICE/
│   └── ESP32_S3_IO_DEVICE.ino
├── LICENSE
└── README.md
```

---

## 動作概要

ESP32‑S3 は USB CDC を通じて PC と通信し、Python API から送られる JSON コマンドを解析して I/O を制御します。

### 主なコマンド

- read_di — デジタル入力  
- set_do — デジタル出力  
- read_adc — ADC 読み取り  
- set_pwm — PWM duty 設定  
- **get_pwm_config — PWM 設定の取得（freq / res）**  
- **set_pwm_config — PWM 設定の変更（NVS に保存）**  
- get_io_state — 全 I/O 状態の取得  
- get_status — デバイス状態の取得  
- help — コマンド一覧を取得  
- ping — 通信確認用コマンド  

---

## PWM 設定について（freq / res）

ESP32IO Firmware では、PWM の周波数（freq）と分解能（res）を自由に設定できます。

- 設定値は **Preferences（NVS）に保存**され、電源再投入後も保持されます  
- 不正な設定を防ぐため、以下の範囲でバリデーションしています  

| 項目 | 許容範囲 | 説明 |
|------|----------|------|
| freq | 1〜20000 Hz | LED やサーボ用途で安全に動作 |
| res | 1〜16 bit | Arduino-ESP32 LEDC API の安全範囲 |

---

## ビルド方法（PlatformIO）

1. VSCode + PlatformIO をインストール  
2. 本リポジトリを開く  
3. 左側の PlatformIO メニューから **Build** を実行  
4. ESP32‑S3 を USB 接続して **Upload** を実行  

ESP32‑S3 DevKitC‑1 を想定しています。

---

## JSON コマンド例

### PWM 設定の変更

```json
{"cmd": "set_pwm_config", "freq": 5000, "res": 8}
```

### PWM 設定の取得

```json
{"cmd": "get_pwm_config"}
```

### ADC 読み取り

```json
{"cmd": "read_adc", "pin_id": 0}
```

レスポンス例：

```json
{"status": "ok", "value": 1234}
```

---

## Python API との連携

このファームウェアは、以下の Python API と連携して動作します：

https://github.com/noritama-lab/esp32io-api

---

## ライセンス

MIT License  
Copyright (c) 2026 Noritama-Lab

---

# 🇺🇸 ESP32IO Firmware

ESP32IO Firmware turns an ESP32‑S3 into a USB‑connected I/O device.  
It works together with the ESP32IO Python API to provide safe and intuitive I/O control over USB serial.

### Features

- USB CDC communication with PC  
- JSON‑based command protocol  
- Supports ADC / PWM / DIO (input/output)  
- Unified response format and error handling  
- **Configurable PWM frequency and resolution (stored in NVS)**  
- Command set aligned with the Python API  
- Works immediately when connected via USB  

---

## Repository Structure

```
esp32io-firmware/
├── ESP32_S3_IO_DEVICE/
│   └── ESP32_S3_IO_DEVICE.ino
├── LICENSE
└── README.md
```

---

## How It Works

The ESP32‑S3 communicates with the PC via USB CDC.  
It receives JSON commands from the Python API, processes them, and controls the I/O pins accordingly.

### Main Commands

- read_di — Digital input  
- set_do — Digital output  
- read_adc — Read ADC  
- set_pwm — Set PWM duty  
- **get_pwm_config — Get PWM frequency/resolution**  
- **set_pwm_config — Set PWM frequency/resolution (stored in NVS)**  
- get_io_state — Get all I/O states  
- get_status — Get device status  
- help — List available commands  
- ping — Communication check  

---

## PWM Configuration (freq / res)

PWM frequency and resolution can be configured and stored persistently.

| Item | Range | Notes |
|------|-------|-------|
| freq | 1–20000 Hz | Safe for LEDs and servos |
| res  | 1–16 bit | Safe range for Arduino‑ESP32 LEDC |

---

## Build Instructions (PlatformIO)

1. Install VSCode + PlatformIO  
2. Open this repository  
3. Run **Build**  
4. Connect ESP32‑S3 and run **Upload**  

Designed for ESP32‑S3 DevKitC‑1.

---

## JSON Command Example

```json
{"cmd": "read_adc", "pin_id": 0}
```

Response:

```json
{"status": "ok", "value": 1234}
```

---

## Python API

This firmware works with the following Python API:

https://github.com/noritama-lab/esp32io-api

---

## License

MIT License  
Copyright (c) 2026 Noritama-Lab