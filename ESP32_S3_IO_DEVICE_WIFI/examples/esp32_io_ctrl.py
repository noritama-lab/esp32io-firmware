"""
ESP32-S3 Serial API Control Example

USB CDC(Serial)経由でESP32に接続し、JSONメッセージを使用してIO操作を行うサンプルです。
"""
import serial
import json
import time

class ESP32IODevice:
    """
    シリアル(USB)を使用してESP32デバイスを制御するクラス
    """
    def __init__(self, port, baudrate=115200, timeout=2):
        """
        初期化とシリアルポートのオープン
        :param port: ポート名 (例: 'COM9', '/dev/ttyACM0')
        :param baudrate: ボーレート
        :param timeout: タイムアウト(秒)
        """
        self.ser = serial.Serial(port, baudrate, timeout=timeout)
        # DTRを制御して、シリアル接続時のESP32不意のリセットを防止
        self.ser.dtr = False
        self.ser.rts = False
        # ESP32の起動メッセージが出力完了するのを待機
        time.sleep(1.0)
        self.ser.reset_input_buffer()

    def send_command(self, cmd_dict):
        """
        JSONコマンドを送信してレスポンスを受け取る
        :param cmd_dict: コマンド辞書
        :return: レスポンス辞書
        """
        try:
            self.ser.reset_input_buffer() # 各コマンド送信前にバッファをクリア
            line = json.dumps(cmd_dict) + "\n"
            start_time = time.perf_counter()
            self.ser.write(line.encode('utf-8'))
            # レスポンスの読み取り
            response = self.ser.readline().decode('utf-8').strip()
            end_time = time.perf_counter()
            elapsed = (end_time - start_time) * 1000
            print(f"  [Response Time: {elapsed:.2f} ms]")

            if response:
                return json.loads(response)
            return {"status": "error", "message": "No response"}
        except Exception as e:
            return {"status": "error", "message": str(e)}

    def close(self):
        self.ser.close()

if __name__ == "__main__":
    # ポート番号は自身の環境に合わせて変更してください
    PORT = "COM9" 
    device = ESP32IODevice(PORT)

    print("--- 1. Ping test ---")
    # {"cmd": "ping"} を送信。 {"status": "ok", "message": "pong"} が返れば成功
    res = device.send_command({"cmd": "ping"})
    print(f"Response: {res}")

    print("\n--- 2. Read Digital Input (Pin 0: GPIO 4) ---")
    di_res = device.send_command({"cmd": "read_di", "pin_id": 0})
    if di_res.get("status") == "ok":
        print(f"  DI[0] State: {'HIGH' if di_res.get('value') else 'LOW'}")

    print("\n--- 3. Read ADC Input (Pin 0: GPIO 1) ---")
    # ADC 0番ピン（GPIO 1）を読み取り
    adc_res = device.send_command({"cmd": "read_adc", "pin_id": 0})
    if adc_res.get("status") == "ok":
        print(f"  ADC[0] Value: {adc_res.get('value')}")

    print("\n--- 4. Set PWM Configuration (8bit) ---")
    # 255を最大値にするため8bitに設定
    device.send_command({"cmd": "set_pwm_config", "freq": 1000, "res": 8})

    print("\n--- 5. PWM Duty Cycle Sweep (Pin 1: GPIO 39) ---")
    # 徐々に明るくするような動作（0% -> 100%）
    max_duty = 255 # デフォルト8bit想定
    for d in range(0, max_duty + 1, 64):
        print(f"  PWM[1] Duty: {d}")
        device.send_command({"cmd": "set_pwm", "pin_id": 1, "duty": d})
        time.sleep(0.2)
    
    # 消灯
    device.send_command({"cmd": "set_pwm", "pin_id": 1, "duty": 0})

    print("\n--- 6. RGB LED Control (NeoPixel) ---")
    # 水色 (R:0, G:255, B:255) に設定
    print("  Setting LED to Cyan...")
    res = device.send_command({"cmd": "set_rgb", "r": 0, "g": 255, "b": 255, "brightness": 32})
    print(f"  Response: {res}")

    # 状態の確認
    led_state = device.send_command({"cmd": "get_led_state"})
    print(f"  Current LED State: {led_state}")

    time.sleep(2)

    # 消灯（ステータス表示モードに戻る）
    print("  Turning LED off (Restoring Status Mode)...")
    device.send_command({"cmd": "led_off"})

    print("\nClosing serial port.")
    device.close()