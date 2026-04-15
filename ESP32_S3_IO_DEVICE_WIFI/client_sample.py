import json
import requests
import serial
import time

class ESP32IOClient:
    """ESP32-S3 IO デバイス操作用クライアント"""
    
    def __init__(self, mode='http', url='http://192.168.4.1/api', port='COM3'):
        self.mode = mode
        self.url = url
        self.port = port
        self.ser = None

        if self.mode == 'serial':
            # シリアルポートを一度だけ開く（リセット防止）
            self.ser = serial.Serial(self.port, 115200, timeout=1)
            # DTRをFalseにすることでリセットを抑制できる場合がある（環境依存）
            self.ser.dtr = False 
            time.sleep(2) # 起動完了待ち

    def send(self, cmd_dict):
        """コマンドをJSONで送信して結果を返す"""
        if self.mode == 'http':
            try:
                resp = requests.post(self.url, json=cmd_dict, timeout=2)
                return resp.json()
            except Exception as e:
                return {"status": "error", "message": str(e)}
        else:
            # シリアル通信
            try:
                if not self.ser or not self.ser.is_open:
                    return {"status": "error", "message": "serial not open"}
                
                self.ser.write((json.dumps(cmd_dict) + '\n').encode())
                line = self.ser.readline().decode().strip()
                return json.loads(line) if line else {"status": "error", "message": "no response"}
            except Exception as e:
                return {"status": "error", "message": str(e)}

    def close(self):
        """終了時にポートを閉じる"""
        if self.ser and self.ser.is_open:
            self.ser.close()

    def __del__(self):
        self.close()

def run_demo():
    # 接続先設定に合わせて変更してください
    # WiFi AP接続時のデフォルトは HTTP: 192.168.4.1
    client = ESP32IOClient(mode='http') 
    # シリアル通信の場合は mode='serial' を指定し、port をお使いの環境に合わせて変更します
    #client = ESP32IOClient(mode='serial', port='COM4') 

    print("--- 1. ステータス確認 ---")
    print(client.send({"cmd": "get_status"}))

    print("\n--- 2. RGB LED 操作 (Red) ---")
    client.send({"cmd": "set_rgb", "r": 255, "g": 0, "b": 0, "brightness": 64})
    time.sleep(1)

    print("--- RGB LED 操作 (Green) ---")
    client.send({"cmd": "set_rgb", "r": 0, "g": 255, "b": 0})
    time.sleep(1)

    print("--- LED OFF ---")
    client.send({"cmd": "led_off"})

    print("\n--- 3. デジタル出力 (DO 0) を点滅 ---")
    for _ in range(3):
        client.send({"cmd": "set_do", "pin_id": 0, "value": 1})
        time.sleep(0.3)
        client.send({"cmd": "set_do", "pin_id": 0, "value": 0})
        time.sleep(0.3)

    print("\n--- 4. アナログ入力 (ADC 0) の読み取り ---")
    adc_res = client.send({"cmd": "read_adc", "pin_id": 0})
    print(f"ADC Value: {adc_res.get('value')}")

    print("\n--- 5. PWM 設定変更と出力 ---")
    # 周波数を 10kHz に設定
    client.send({"cmd": "set_pwm_config", "freq": 10000, "res": 10})
    # Duty設定 (10bitなので 0-1023)
    client.send({"cmd": "set_pwm", "pin_id": 0, "duty": 512})
    print("PWM 50% 出力開始")
    time.sleep(1)
    client.send({"cmd": "set_pwm", "pin_id": 0, "duty": 0})

    print("\n--- 6. IO状態の全取得 ---")
    print(json.dumps(client.send({"cmd": "get_io_state"}), indent=2))

if __name__ == "__main__":
    # 実行前に pip install requests pyserial が必要です
    print("ESP32-S3 IO Device Demo Start")
    run_demo()
    print("\nDemo Finished")