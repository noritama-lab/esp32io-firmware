import serial
import time
import json

SERIAL_PORT = 'COM4'  # 適宜変更
BAUDRATE = 115200

with serial.Serial(SERIAL_PORT, BAUDRATE, timeout=1) as ser:
    # PWM0を128に設定（JSON形式）
    cmd = {'cmd': 'set_pwm', 'pin_id': 0, 'duty': 128}
    t0 = time.time()
    ser.write((json.dumps(cmd) + '\n').encode())
    resp = ser.readline().decode(errors='ignore').strip()
    t1 = time.time()
    print(f'PWM応答: {resp}  ({(t1-t0)*1000:.1f} ms)')

    # WiFi設定（JSON形式）
    cmd = {'cmd': 'get_status'}
    t0 = time.time()
    ser.write((json.dumps(cmd) + '\n').encode())
    resp = ser.readline().decode(errors='ignore').strip()
    t1 = time.time()
    print(f'ステータス: {resp}  ({(t1-t0)*1000:.1f} ms)')

    # 設定取得（JSON形式）
    cmd = {'cmd': 'ping'}
    t0 = time.time()
    ser.write((json.dumps(cmd) + '\n').encode())
    resp = ser.readline().decode(errors='ignore').strip()
    t1 = time.time()
    print(f'ping応答: {resp}  ({(t1-t0)*1000:.1f} ms)')
