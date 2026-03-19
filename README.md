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

- read_dio — デジタル入力  
- write_dio — デジタル出力  
- read_adc — ADC 読み取り  
- set_pwm — PWM 出力  
- get_io_state — 全 I/O 状態の取得  
- get_status — デバイス状態の取得
- help — コマンド一覧を取得
- ping — 通信確認用コマンド

---

## ビルド方法（PlatformIO）

1. VSCode + PlatformIO をインストール  
2. 本リポジトリを開く  
3. 左側の PlatformIO メニューから **Build** を実行  
4. ESP32‑S3 を USB 接続して **Upload** を実行  

ESP32‑S3 DevKitC‑1 を想定しています。

---

## JSON コマンド例

```json
{"cmd": "read_adc", "pin": 0}
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

- read_dio — Digital input
- write_dio — Digital output
- read_adc — Read ADC
- set_pwm — PWM output
- get_io_state — Get all I/O states
- get_status — Get device status
- help — List available commands
- ping — Communication check

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
{"cmd": "read_adc", "pin": 0}
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
