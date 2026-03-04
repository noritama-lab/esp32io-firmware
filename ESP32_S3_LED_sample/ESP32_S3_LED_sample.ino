// ESP32‑S3 LED 点滅テスト
const int LED_PIN = 44;     // 使用するPIN番号

void setup() {
  pinMode(LED_PIN, OUTPUT);     // 選択PIN番号を出力に設定
}

void loop() {
  digitalWrite(LED_PIN, HIGH);  // 点灯
  delay(500);
  digitalWrite(LED_PIN, LOW);   // 消灯
  delay(500);
}