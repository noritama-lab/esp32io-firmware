// ESP32‑S3 LED 点滅テスト
const int LED_PIN = 13;   // 例：DevKitC-1 は GPIO13

void setup() {
  pinMode(LED_PIN, OUTPUT);
}

void loop() {
  digitalWrite(LED_PIN, HIGH);  // 点灯
  delay(500);
  digitalWrite(LED_PIN, LOW);   // 消灯
  delay(500);
}