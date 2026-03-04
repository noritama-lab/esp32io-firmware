// LED を接続しているピン番号
const int LED_PIN = 5;
// ボタンを接続しているピン番号（プルアップ入力）
const int BUTTON_PIN = 10;

void setup() {
  // LED ピンを出力に設定
  pinMode(LED_PIN, OUTPUT);

  // ボタンピンを内部プルアップ付き入力に設定
  // → ボタンが押されていないと HIGH、押されると LOW になる
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // シリアルモニタ用の通信速度を設定
  Serial.begin(115200);
}

void loop() {
  // ボタンの状態を読み取る（HIGH or LOW）
  int state = digitalRead(BUTTON_PIN);

  // ボタンが押されているとき（LOW）
  if (state == LOW) {
    digitalWrite(LED_PIN, HIGH);     // LED を点灯
    Serial.println("PRESSED");       // シリアルに「PRESSED」と表示
  } else {
    digitalWrite(LED_PIN, LOW);      // LED を消灯
    Serial.println("RELEASED");      // シリアルに「RELEASED」と表示
  }

  // 状態を読みやすくするための 100ms 待機
  delay(100);
}
