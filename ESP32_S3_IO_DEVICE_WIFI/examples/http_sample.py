import requests
import time

ESP32_IP = '172.20.10.14'  # 適宜書き換え

# PWM0を128に設定
t0 = time.time()
r = requests.get(f'http://{ESP32_IP}/api', params={'cmd': 'set_pwm', 'pin_id': 0, 'duty': 128})
t1 = time.time()
print(f'PWM: {r.text}  ({(t1-t0)*1000:.1f} ms)')

# WiFi設定
t0 = time.time()
r = requests.post(f'http://{ESP32_IP}/api', data={'cmd': 'get_status'})
t1 = time.time()
print(f'ステータス: {r.text}  ({(t1-t0)*1000:.1f} ms)')

# 設定取得
t0 = time.time()
r = requests.get(f'http://{ESP32_IP}/api', params={'cmd': 'ping'})
t1 = time.time()
print(f'ping応答: {r.text}  ({(t1-t0)*1000:.1f} ms)')
