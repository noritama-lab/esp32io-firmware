"""
ESP32-S3 HTTP API Control Example

WiFi経由でESP32に接続し、HTTP GET/POST JSONを使用してIO操作を行うサンプルです。
"""
import requests
import json
import time

class ESP32HTTPDevice:
    """
    HTTP APIを使用してESP32デバイスを制御するクラス
    """
    def __init__(self, ip_address):
        """
        初期化
        :param ip_address: ESP32のIPアドレス
        """
        self.base_url = f"http://{ip_address}/api"

    def send_command(self, cmd_dict, method="GET"):
        """
        コマンドを送信する (GETパラメータまたはPOST JSONに対応)
        :param cmd_dict: 送信するコマンドの辞書
        :param method: "GET" または "POST"
        :return: レスポンスの辞書
        """
        try:
            start_time = time.perf_counter()
            
            if method.upper() == "POST":
                # JSONボディとして送信
                response = requests.post(self.base_url, json=cmd_dict, timeout=3)
            else:
                # クエリパラメータとして送信
                response = requests.get(self.base_url, params=cmd_dict, timeout=3)
                
            end_time = time.perf_counter()
            
            elapsed = (end_time - start_time) * 1000
            print(f"  [HTTP Response Time: {elapsed:.2f} ms]")

            if response.status_code == 200:
                return response.json()
            else:
                return {"status": "error", "message": f"HTTP {response.status_code}"}
        except Exception as e:
            return {"status": "error", "message": str(e)}

if __name__ == "__main__":
    # ESP32のシリアルモニタに表示されたIPアドレスに書き換えてください
    ESP32_IP = "192.168.0.20" 
    device = ESP32HTTPDevice(ESP32_IP)

    print("\n--- 1. Ping test ---")
    res = device.send_command({"cmd": "ping"})
    print(f"  Result: {res}")

    print("\n--- 2. Device Status ---")
    status = device.send_command({"cmd": "get_status"})
    if status.get("status") == "ok":
        print(f"  Uptime: {status.get('uptime_ms')} ms")
        print(f"  Free Heap: {status.get('free_heap')} bytes")
        print(f"  WiFi IP: {status.get('wifi_ip')}")

    print("\n--- 3. Read All IO States ---")
    state = device.send_command({"cmd": "get_io_state"})
    if state.get("status") == "ok":
        print(f"  ADC Values : {state.get('adc')}")
        print(f"  Digital In : {state.get('dio_in')}")
        print(f"  Digital Out: {state.get('dio_out')}")
        print(f"  PWM Duties : {state.get('pwm')}")

    print("\n--- 4. Digital Output Toggle (Pin 0: GPIO 10) ---")
    for val in [1, 0]:
        print(f"  Setting DO 0 to {'HIGH' if val else 'LOW'}...")
        res = device.send_command({"cmd": "set_do", "pin_id": 0, "value": val})
        print(f"  Response: {res}")
        time.sleep(0.5)

    print("\n--- 5. PWM Configuration & Output (Pin 0: GPIO 38) ---")
    # PWM設定の変更 (5kHz, 10bit)
    conf_res = device.send_command({"cmd": "set_pwm_config", "freq": 5000, "res": 10})
    print(f"  Config Response: {conf_res}")

    if conf_res.get("status") == "ok":
        max_duty = conf_res.get("max_duty", 1023)
        # 50% Dutyで出力
        target_duty = max_duty // 2
        print(f"  Setting PWM 0 to duty {target_duty} (50%)...")
        res = device.send_command({"cmd": "set_pwm", "pin_id": 0, "duty": target_duty})
        print(f"  Response: {res}")
        
        time.sleep(1)
        
        # 停止
        device.send_command({"cmd": "set_pwm", "pin_id": 0, "duty": 0})
        print("  PWM Output Stopped.")

    print("\n--- 6. RGB LED Control (NeoPixel) ---")
    # 紫色 (R:255, G:0, B:255) に設定
    print("  Setting LED to Purple...")
    res = device.send_command({"cmd": "set_rgb", "r": 255, "g": 0, "b": 255, "brightness": 64})
    print(f"  Response: {res}")

    # 状態の確認
    led_state = device.send_command({"cmd": "get_led_state"})
    print(f"  Current LED State: {led_state}")

    time.sleep(2)

    # 消灯（ステータス表示モードに戻る）
    print("  Turning LED off (Restoring Status Mode)...")
    device.send_command({"cmd": "led_off"})
    
    print("\nDemo Finished.")