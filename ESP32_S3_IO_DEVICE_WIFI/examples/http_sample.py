import requests
import time

ESP32_IP = '192.168.4.1'  # 適宜書き換え

# PWM0を128に設定
t0 = time.time()
r = requests.get(f'http://{ESP32_IP}/api', params={'cmd': 'pwm', 'ch': 0, 'duty': 128})
t1 = time.time()
print(f'PWM: {r.text}  ({(t1-t0)*1000:.1f} ms)')

# WiFi設定
t0 = time.time()
r = requests.post(f'http://{ESP32_IP}/api', data={'cmd': 'wifi', 'ssid': 'mywifi', 'pass': 'mypass'})
t1 = time.time()
print(f'WiFi設定: {r.text}  ({(t1-t0)*1000:.1f} ms)')

# 設定取得
t0 = time.time()
r = requests.get(f'http://{ESP32_IP}/api', params={'cmd': 'get'})
t1 = time.time()
print(f'設定取得: {r.text}  ({(t1-t0)*1000:.1f} ms)')
