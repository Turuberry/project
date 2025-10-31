#include <Arduino_BMI270_BMM150.h>
#include <ArduinoBLE.h>
#include <math.h>

// 핀 설정
const int flexPins[5]  = {A0, A1, A2, A3, A4};
const int buzzerPin    = 7;
const int ledPin       = LED_BUILTIN;
const int switchPins[3] = {4, 5, 3};

// BLE 설정
BLEService gloveService("180C");
BLECharacteristic gloveChar("2A99", BLENotify, 120);

int   minVals[5];
int   maxVals[5];
int   rawVals[5];
float initialAcc[3];
bool  wasReversed = false;

// -------------------- 유틸 함수 --------------------
void playBuzzer(int duration) {
  digitalWrite(buzzerPin, HIGH);
  delay(duration);
  digitalWrite(buzzerPin, LOW);
}

void blinkLED(int times, int interval) {
  for (int i = 0; i < times; i++) {
    digitalWrite(ledPin, HIGH);
    delay(interval);
    digitalWrite(ledPin, LOW);
    delay(interval);
  }
}

float cosineSimilarity(float* v1, float* v2) {
  float dot = 0, norm1 = 0, norm2 = 0;
  for (int i = 0; i < 3; i++) {
    dot   += v1[i] * v2[i];
    norm1 += v1[i] * v1[i];
    norm2 += v2[i] * v2[i];
  }
  return dot / (sqrt(norm1) * sqrt(norm2) + 1e-6);
}

// -------------------- setup() --------------------
void setup() {
  Serial.begin(9600);
  while (!Serial && millis() < 5000) { /* wait for serial */ }

  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, LOW);
  pinMode(ledPin, OUTPUT);

  for (int i = 0; i < 3; i++) {
    pinMode(switchPins[i], INPUT_PULLUP);
  }

  if (!IMU.begin()) {
    Serial.println("❌ IMU 초기화 실패");
    while (1) { /* halt */ }
  }
  Serial.println("✅ IMU 초기화 완료");

  if (!BLE.begin()) {
    Serial.println("❌ BLE 초기화 실패");
    while (1) { /* halt */ }
  }

  BLE.setLocalName("FlexIMUGlove");
  gloveService.addCharacteristic(gloveChar);
  BLE.addService(gloveService);
  BLE.advertise();
  Serial.println("BLE 광고 시작됨");

  digitalWrite(ledPin, HIGH);
  playBuzzer(300);
  delay(5000);
  digitalWrite(ledPin, LOW);
  delay(2000);

  // 캘리브레이션: 펴기
  playBuzzer(300);
  Serial.println("손가락을 펴고 계세요! (3초 후 측정)");
  delay(3000);
  for (int i = 0; i < 5; i++) {
    minVals[i] = analogRead(flexPins[i]);
  }
  delay(5000);

  // 캘리브레이션: 구부리기
  playBuzzer(300);
  Serial.println("손가락을 최대한 구부리세요! (3초 후 측정)");
  delay(3000);
  for (int i = 0; i < 5; i++) {
    maxVals[i] = analogRead(flexPins[i]);
  }
  delay(5000);

  playBuzzer(300);
  blinkLED(2, 500);
  Serial.println(" 캘리브레이션 완료. 중력 반전 시 데이터 전송 시작.");

  // 초기 중력 벡터 저장
  IMU.readAcceleration(initialAcc[0], initialAcc[1], initialAcc[2]);
}

// -------------------- loop() --------------------
void loop() {
  float ax = 0, ay = 0, az = 0;
  if (IMU.accelerationAvailable()) {
    IMU.readAcceleration(ax, ay, az);
  }

  float acc[3] = {ax, ay, az};
  float sim = cosineSimilarity(acc, initialAcc);
  bool isReversed = (sim < -0.6);

  if (isReversed && !wasReversed && BLE.connected()) {
    int flexLevel[5];

    for (int i = 0; i < 5; i++) {
      int raw = analogRead(flexPins[i]);
      rawVals[i] = raw;

      int val  = constrain(raw, minVals[i], maxVals[i]);    // 0: 구부림, 100: 펴짐
      int flex = map(val, maxVals[i], minVals[i], 0, 100);
      flex     = constrain(flex, 0, 100);
      flexLevel[i] = flex;
    }

    int buttonVals[3];
    for (int i = 0; i < 3; i++) {   
      buttonVals[i] = (digitalRead(switchPins[i]) == HIGH) ? 30 : 0; // HIGH = 미눌림(30), LOW = 눌림(0)

    String msg = "FLEX:";
    for (int i = 0; i < 5; i++) {
      msg += String(flexLevel[i]);
      msg += (i < 4 ? "," : ";");
    }
    msg += "BTN:" + String(buttonVals[0]) + "," + String(buttonVals[1]) + "," + String(buttonVals[2]) + ";";

    gloveChar.setValue(msg.c_str());
    Serial.println("\n전송: " + msg);

    playBuzzer(200);
    wasReversed = true;
  } else if (!isReversed) {
    wasReversed = false;
  }

  BLE.poll();
  delay(100);
}
