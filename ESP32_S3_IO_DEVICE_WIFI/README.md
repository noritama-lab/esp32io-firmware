# ESP32_S3_IO_DEVICE

## 概要
ESP32 S3を使った多機能I/Oデバイスのファームウェアです。Web設定画面・HTTP API・WiFi固定IP・BOOT長押しリセット等をサポートしています。

## 主な機能
- WebブラウザからWiFi設定・IPアドレス・PWM出力等を設定可能
- HTTP APIで外部からI/O制御・状態取得
- AP+STA同時モード（設定用APと通常WiFi同時利用）
- BOOTボタン2秒長押しで全設定リセット
- 設定値はPreferencesに保存
- セキュリティ強化（APパスワード必須、HTMLエスケープ等）

## 使い方
1. ESP32を起動すると、設定済みWiFiに自動接続します。
2. 未設定または接続失敗時はAPモード（SSID: ESP32_S3_IO_SETUP, パスワード: esp32setup）で起動。
3. スマホやPCでAPに接続し、ブラウザで http://192.168.4.1/ を開き設定。
4. 設定保存後、WiFi接続に成功すれば自動再起動。
5. BOOTボタン2秒長押しで全設定リセット。

## HTTP API
- /api?cmd=... でGET/POSTリクエスト
- 例: /api?cmd=pwm&ch=0&duty=128
- 詳細はソースコード内コメント参照

## 注意事項
- API認証なし（LAN内利用推奨）
- 設定値はPreferencesに平文保存
- /save画面のHTMLエスケープは限定的

## ファイル構成
- ESP32_S3_IO_DEVICE.ino ... メインファームウェア

## ライセンス
MIT License