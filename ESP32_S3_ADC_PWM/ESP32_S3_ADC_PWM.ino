// PWM 出力に使う GPIO ピン番号
const int LED_PIN = 5;

// アナログ入力に使う GPIO ピン番号（0〜4095 の ADC 値）
const int ANALOG_PIN = 10;

void setup() {
  Serial.begin(115200);

  // LEDC（PWM）初期化
  // 第1引数: PWM を出力するピン番号
  // 第2引数: PWM 周波数（Hz）
  // 第3引数: 分解能（ビット数）→ 8bit なら 0〜255
  //
  // esp32 v3.x では ledcAttachPin() ではなく ledcAttach() を使う。
  ledcAttach(LED_PIN, 5000, 8);
}

void loop() {
  // アナログ入力を読み取る（0〜4095:12bit）
  int sensorValue = analogRead(ANALOG_PIN);

  // 読み取った値を PWM の 0〜255 に変換
  // 0 → 最小デューティ
  // 4095 → 最大デューティ
  int pwmValue = map(sensorValue, 0, 4095, 0, 255);

  // PWM 出力
  // esp32 v3.x では ledcWrite() に「チャンネル番号」ではなく「ピン番号」を渡す
  ledcWrite(LED_PIN, pwmValue);

  // デバッグ出力
  Serial.print("Analog: ");
  Serial.print(sensorValue);
  Serial.print("  PWM: ");
  Serial.println(pwmValue);

  delay(20);  // 更新間隔（20ms）
}