import serial
import json
import time

class ESP32IODevice:
    def __init__(self, port, baudrate=115200, timeout=2):
        """
        port: Windowsの場合は 'COM3', Linux/Macの場合は '/dev/ttyACM0' など
        """
        self.ser = serial.Serial(port, baudrate, timeout=timeout)
        # ESP32-S3のUSB CDCが安定するまで少し待機
        time.sleep(0.5)
        self.ser.reset_input_buffer()

    def send_command(self, cmd_dict):
        """JSONコマンドを送信してレスポンスを受け取る"""
        try:
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
    PORT = "COM4" 
    device = ESP32IODevice(PORT)

    print("--- 1. Ping test ---")
    # {"cmd": "ping"} を送信。 {"status": "ok", "message": "pong"} が返れば成功
    res = device.send_command({"cmd": "ping"})
    print(f"Response: {res}")

    print("\n--- 2. Get IO State ---")
    state = device.send_command({"cmd": "get_io_state"})
    if state.get("status") == "ok":
        print(f"ADC Pins: {state.get('adc')}")
        print(f"Digital In: {state.get('dio_in')}")
    else:
        print(f"Error: {state}")

    print("\n--- 3. RGB LED Control (Red) ---")
    # 赤色(R=255, G=0, B=0), 明るさ50で点灯
    led_res = device.send_command({"cmd": "set_rgb", "r": 255, "g": 0, "b": 0, "brightness": 50})
    print(f"LED Response: {led_res}")
    time.sleep(1)

    print("\n--- 4. LED Off ---")
    print(device.send_command({"cmd": "led_off"}))

    device.close()