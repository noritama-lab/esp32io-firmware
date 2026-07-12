# ESP32IO コマンドリファレンス

対象: `ESP32_S3_IO_DEVICE/` ファームウェア。
デバイス制御は **USBシリアル** と **HTTP** の両方から、共通のJSONコマンドセットで行えます。

| 方法 | 用途 |
|---|---|
| **USBシリアル (CDC)** | PC/SBCとUSB接続した直接制御。Wi-Fi設定不要でケーブル1本で完結 |
| **HTTP JSON API** (`/api`) | Wi-Fi経由のネットワーク制御。curl / Python / Node-RED / ブラウザ等どこからでも利用可能 |

---

## 0. 共通事項

- デバイス名: `ESP32_S3_IO_XXXXXX`（`XXXXXX`はMACアドレス下3バイト）
  - Wi-Fi AP SSID、mDNSホスト名、設定ページのタイトルに使われます
  - 初回起動時はAPモードで `http://192.168.4.1/` から接続（AP パスワード: `esp32setup`）
- USBシリアル: **115200bps**、改行 (`\n`) 区切りで1行1JSON。レスポンスも1行のJSONで返却されます
- HTTP: `POST /api`（JSONボディ）と `GET /api?cmd=...&param=value`（クエリパラメータ）の両方に対応
  - GETのクエリパラメータは数値と判定できる値（例: `pin_id=1`）が自動的に数値へキャストされます
  - 配列を渡す必要があるコマンド（`i2c_write`の`data`など）は **POSTのJSONボディのみ** 対応

---

## 1. ピンマッピング

| 機能 | pin_id範囲 | GPIO |
|---|---|---|
| Digital Input (DI) | 0–5 | 4, 5, 6, 7, 8, 9 |
| Digital Output (DO) | 0–5 | 10, 11, 12, 13, 14, 15 |
| ADC Input | 0–1 | 1, 2 |
| PWM Output | 0–1 | 36, 37 |
| RGB Status LED (NeoPixel) | - | 38 |
| I2C SDA / SCL | - | 40 / 41 |
| BOOT / Factory Reset Button | - | 0 |

- ADC値は12bit (0–4095) を8回平均したものです（`ADC_SAMPLES=8`）
- PWMデューティの最大値は現在の分解能に依存します（`get_pwm_config`の`max_duty`を参照。デフォルト: 5000Hz / 8bit → 最大255）
- Digital Output の論理は設定ページの「DIO Output Logic」で正論理/負論理を切替可能です（`read_di`/`set_do`は常に論理値 1=ON / 0=OFF で扱われます）
- BOOTボタンを5秒間長押しするとNVS設定を消去してファクトリーリセットします

---

## 2. レスポンス形式

成功時: `{"status":"ok", ...コマンド固有のフィールド}`
失敗時: `{"status":"error","cmd":"<コマンド名>","code":"<エラーコード>","detail":"<説明>"}`

| エラーコード | 意味 |
|---|---|
| `ERR_UNKNOWN` | 存在しないコマンド名（未知のコマンド、または`cmd`欠落） |
| `ERR_PARAM` | 必須パラメータ不足（例: `i2c_write`の`data`欠落） |
| `ERR_RANGE` | `pin_id`が範囲外 |
| `ERR_INVALID_MODE` | `set_led_mode`の`mode`が`status`/`manual`以外 |

> 送信したJSON自体が不正な場合、HTTPでは`cmd`が空として扱われ`ERR_UNKNOWN`が返ります。USBシリアルではJSONの解析に失敗した行は無視され、応答は返りません。

---

## 3. コマンド一覧

### Digital IO / ADC / PWM

| コマンド | パラメータ | レスポンス | 説明 |
|---|---|---|---|
| `read_di` | `pin_id` (0–5) | `value` (0/1) | デジタル入力を読み取り |
| `set_do` | `pin_id` (0–5), `value` (0/1) | - | デジタル出力を設定 |
| `get_io_state` | - | `dio_in[6]`, `dio_out[6]`, `adc[2]`, `pwm[2]` | 全IO状態を一括取得 |
| `read_adc` | `pin_id` (0–1) | `value` (0–4095) | ADC値を読み取り |
| `set_pwm` | `pin_id` (0–1), `duty` | `duty` | PWMデューティを設定 |
| `get_pwm_config` | - | `freq`, `res`, `max_duty` | 現在のPWM周波数/分解能/最大デューティを取得 |
| `set_pwm_config` | `freq` (既定5000), `res` (既定8) | - | PWM周波数/分解能を再設定（デューティは自動スケーリング） |

### I2C

| コマンド | パラメータ | レスポンス | 説明 |
|---|---|---|---|
| `i2c_scan` | - | `devices[]` | バス上のI2Cアドレスを検出 |
| `i2c_write` | `addr`, `data[]` (最大64バイト) | `i2c_err` (`Wire.endTransmission()`の戻り値) | 指定アドレスへ書き込み |
| `i2c_read` | `addr`, `len` (最大64) | `data[]`, `len` | 指定アドレスから読み取り |

### センサー / OLED

| コマンド | パラメータ | レスポンス | 説明 |
|---|---|---|---|
| `get_sensors` | - | `inventory{bme280,mpu6050,vl53l1x,oled}` (bool)、検出済みなら `bme280{temp,hum,press}` / `mpu6050{accel[3],gyro[3]}` / `vl53l1x{distance}` | 接続中の全センサーデータを一括取得 |
| `set_oled` | `text`, `x` (既定0), `y` (既定0), `size` (既定1), `clear` (既定true) | - | OLEDにテキストを表示 |

### RGB LED

| コマンド | パラメータ | レスポンス | 説明 |
|---|---|---|---|
| `set_rgb` | `r`,`g`,`b`,`brightness` (各0–255) | `r`,`g`,`b`,`brightness` | LED色を設定（自動的にmanualモードへ） |
| `led_off` | - | - | LED消灯（manualモードへ） |
| `set_led_mode` | `mode` (`"status"` \| `"manual"`) | `mode` | LED動作モードを切替（`status`=WiFi状態表示、`manual`=API制御） |
| `get_led_state` | - | `on`(bool),`r`,`g`,`b`,`brightness` | 現在のLED状態を取得 |

### システム

| コマンド | パラメータ | レスポンス | 説明 |
|---|---|---|---|
| `get_status` | - | `uptime_ms`, `free_heap`, `wifi_connected`, `wifi_ip`, `ap_ip` | デバイスのシステム状態を取得 |
| `ping` | - | `message`:"pong" | 疎通確認 |
| `help` | - | `commands[]` (全19コマンド名) | 利用可能なコマンド一覧を取得（自己文書化API） |

---

## 4. HTTPエンドポイント一覧

| パス | メソッド | 内容 |
|---|---|---|
| `/` | GET | 設定ページ（WiFi設定フォーム + センサーモニター） |
| `/save` | POST | 設定を保存し再起動（フォームPOST、`ssid`必須） |
| `/api` | GET / POST | JSONコマンド実行（上記コマンド一覧） |

---

## 5. 使用例

### USBシリアル（1行JSON）

```
送信: {"cmd":"read_di","pin_id":0}
応答: {"status":"ok","value":1}
```

### curl（HTTP POST）

```bash
curl -X POST http://device.local/api -d '{"cmd":"set_do","pin_id":0,"value":1}'
# => {"status":"ok"}
```

### curl（HTTP GET）

```bash
curl "http://device.local/api?cmd=read_adc&pin_id=0"
# => {"status":"ok","value":2048}
```

### Python

```python
import requests
r = requests.post("http://device.local/api", json={"cmd":"get_sensors"})
print(r.json())
# {"status":"ok","inventory":{"bme280":true,...},"bme280":{"temp":24.8,"hum":51.2,"press":1012.4},...}
```

### エラー例

```json
{"cmd":"read_di","pin_id":9}
```
```json
{"status":"error","cmd":"read_di","code":"ERR_RANGE","detail":"Invalid pin_id"}
```

---

## 関連ドキュメント

- [README.md](README.md) — 概要・セットアップ手順・必要ライブラリ
