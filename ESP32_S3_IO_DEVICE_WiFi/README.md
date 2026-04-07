# ESP32_S3_IO_DEVICE_WiFi

ESP32-S3 を Wi-Fi 経由で制御する I/O デバイス用ファームウェアです。

- Serial JSON コマンドで I/O/PWM を制御
- HTTP API で設定取得・設定変更・I/O状態取得
- WebSocket で Serial と同じ JSON コマンドを利用可能
- Wi-Fi 接続失敗時は AP フォールバック

## ファイル構成

- ESP32_S3_IO_DEVICE_WiFi.ino: 本体ファームウェア
- test_esp32.py: Serial/WebSocket の疎通テスト

## 動作概要

### 1. Wi-Fi 接続

起動時に STA モードで接続を試行します。

- SSID: ソース内の ssid
- Password: ソース内の pass
- タイムアウト: 約 8 秒

接続失敗時は AP モードに切り替わります。

- AP SSID: ESP32-Setup
- AP Password: 12345678
- APアクセス先: http://192.168.4.1

APアクセス先の画面で、接続先 Wi-Fi の SSID と Password を設定できます。
保存後は再起動し、次回起動からその設定で STA 接続を試行します。

### 2. LED 表示

- Wi-Fi 接続中: 高速点滅
- Wi-Fi 接続成功: 点灯
- AP モード: 2回点滅の繰り返し

### 3. 保存される設定

Preferences(NVS) に保存されます。

- dhcp
- ssid
- pass
- ip
- mask
- gw
- pwm_freq
- pwm_res

BOOT(GPIO0) を 3 秒長押しすると NVS 設定をクリアして再起動します。
このとき ssid/pass を含む保存済み設定はすべてリセットされます。

## ピン割り当て

- DIO IN: GPIO 4,5,6,7,8,9
- DIO OUT: GPIO 10,11,12,13,14,15
- ADC: GPIO 1,2
- PWM: GPIO 38,39
- LED: GPIO 21

## Serial JSON コマンド

改行付き JSON を送信します。

例:

```json
{"cmd":"ping"}
```

主なコマンド:

- ping
- help
- get_status
- get_io_state
- read_di
- set_do
- read_adc
- set_pwm
- get_pwm_config
- set_pwm_config

例:

```json
{"cmd":"set_pwm","pin_id":0,"duty":100}
```

```json
{"cmd":"set_pwm_config","freq":1000,"res":8}
```

## HTTP API

### GET /

簡易管理ページを返します。

### GET /api/get_config

設定を JSON で返します。

レスポンス例:

```json
{
  "pwm_freq": 5000,
  "pwm_res": 8,
  "dhcp": true,
  "ip": "192.168.1.50",
  "mask": "255.255.255.0",
  "gw": "192.168.1.1"
}
```

### POST /api/set_network_config

ネットワーク設定を保存して再起動します。

リクエスト例:

```json
{
  "dhcp": true,
  "ip": "192.168.1.50",
  "mask": "255.255.255.0",
  "gw": "192.168.1.1"
}
```

### GET /api/get_io_state

I/O 状態を返します。

### POST /api/set_pwm_config

PWM 周波数・分解能を変更します。

リクエスト例:

```json
{
  "freq": 2000,
  "res": 8
}
```

## WebSocket

WebSocket サーバーは `81` 番ポートで待ち受けます。

- 接続先: `ws://<ESP32のIP>:81/`
- 送受信形式: Serial と同じ 1 メッセージ 1 JSON

送信例:

```json
{"cmd":"ping"}
```

応答例:

```json
{"status":"ok","message":"pong"}
```

使えるコマンドは Serial JSON と同じです。

## テスト方法

Python テストスクリプトを使えます。

依存:

- pyserial
- websocket-client
- colorama (任意)

インストール例:

```bash
pip install pyserial websocket-client colorama
```

実行例:

```bash
python test_esp32.py --port COM3 --host 192.168.1.50
python test_esp32.py --port COM3
python test_esp32.py --host 192.168.1.50
python test_esp32.py --host 192.168.1.50 --ws-port 81
```

## 補足

- 現在の実装は Serial JSON + HTTP API + WebSocket で制御します。
