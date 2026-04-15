import requests
import json
import time

class ESP32HTTPDevice:
    def __init__(self, ip_address):
        self.base_url = f"http://{ip_address}/api"

    def send_command(self, cmd_dict):
        """HTTP GETクエリを使用してコマンドを送信する"""
        try:
            start_time = time.perf_counter()
            # .ino側の実装に合わせてクエリパラメータとして送信
            response = requests.get(self.base_url, params=cmd_dict, timeout=5)
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
    ESP32_IP = "192.168.4.1" 
    device = ESP32HTTPDevice(ESP32_IP)

    print("--- 1. HTTP Ping test ---")
    res = device.send_command({"cmd": "ping"})
    print(f"Response: {res}")

    print("\n--- 2. Get IO State via HTTP ---")
    state = device.send_command({"cmd": "get_io_state"})
    if state.get("status") == "ok":
        # ino側のキー名に合わせて取得 (adc, dio_inなど)
        print(f"ADC Values: {state.get('adc')}")
        print(f"Digital In: {state.get('dio_in')}")
    else:
        print(f"Error: {state}")

    print("\n--- 3. RGB LED Control (Blue) ---")
    # 青色(R=0, G=0, B=255)で点灯
    led_res = device.send_command({
        "cmd": "set_rgb", 
        "r": 0, 
        "g": 0, 
        "b": 255, 
        "brightness": 100
    })
    print(f"LED Response: {led_res}")
    
    time.sleep(2)

    print("\n--- 4. LED Off ---")
    off_res = device.send_command({"cmd": "led_off"})
    print(f"Off Response: {off_res}")

    print("\n--- 5. Set Digital Output (Pin 0 -> High) ---")
    print(device.send_command({"cmd": "set_do", "pin_id": 0, "value": 1}))