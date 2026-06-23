#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include <U8g2lib.h>

Adafruit_ADS1115 ads;

U8G2_SSD1306_128X64_NONAME_F_SW_I2C oled(
  U8G2_R0,
  22,
  21,
  U8X8_PIN_NONE
);

const int GREEN_LED_PIN = 33;
const int RED_LED_PIN = 26;
const int BUZZER_PIN = 27;

const int TILE1 = 0;
const int TILE2 = 1;

const int THRESHOLD = 300;
const unsigned long DEBOUNCE_MS = 400;
const unsigned long MAX_INTERVAL_MS = 5000;

// Final trained values from your 80 samples
const int TRAINED_PEAK1 = 559;
const int TRAINED_PEAK2 = 2386;
const int TRAINED_INTERVAL = 772;

// Balanced tolerances
const int PEAK1_TOLERANCE = 850;
const int PEAK2_TOLERANCE = 4000;
const int INTERVAL_TOLERANCE = 350;

bool waitingForSecondStep = false;

unsigned long firstStepTime = 0;
unsigned long lastStepTime = 0;

int peak1 = 0;
int peak2 = 0;

bool tile1WasPressed = false;
bool tile2WasPressed = false;

void setup() {
  Serial.begin(115200);

  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(GREEN_LED_PIN, LOW);
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  Wire.begin(18, 19);

  oled.begin();
  showOLED("READY", "Tile1 -> Tile2");

  if (!ads.begin()) {
    Serial.println("ADS1115 NOT FOUND");
    showOLED("ADS ERROR", "Check wiring");

    while (1) {
      digitalWrite(RED_LED_PIN, HIGH);
      delay(300);
      digitalWrite(RED_LED_PIN, LOW);
      delay(300);
    }
  }

  ads.setGain(GAIN_ONE);

  Serial.println("=== 2-STEP AUTH SYSTEM READY ===");
  Serial.println("Step Tile1 then Tile2");
}

void loop() {
  int16_t t1 = ads.readADC_SingleEnded(TILE1);
  int16_t t2 = ads.readADC_SingleEnded(TILE2);

  bool tile1Pressed = abs(t1) > THRESHOLD;
  bool tile2Pressed = abs(t2) > THRESHOLD;

  bool tile1Rising = tile1Pressed && !tile1WasPressed;
  bool tile2Rising = tile2Pressed && !tile2WasPressed;

  unsigned long now = millis();

  if (!waitingForSecondStep &&
      tile1Rising &&
      (now - lastStepTime > DEBOUNCE_MS)) {

    peak1 = abs(t1);
    firstStepTime = now;
    lastStepTime = now;
    waitingForSecondStep = true;

    Serial.println();
    Serial.println("Tile1 detected");
    Serial.print("Peak1: ");
    Serial.println(peak1);

    showOLED("STEP TILE 2", "Continue");

    digitalWrite(GREEN_LED_PIN, HIGH);
    delay(150);
    digitalWrite(GREEN_LED_PIN, LOW);
  }

  if (waitingForSecondStep &&
      tile2Rising &&
      (now - lastStepTime > DEBOUNCE_MS)) {

    peak2 = abs(t2);
    unsigned long interval = now - firstStepTime;

    if (interval > MAX_INTERVAL_MS) {
      Serial.println("TOO SLOW");
      showOLED("TOO SLOW", "Try again");

      resetSystem();

      delay(1500);
      showOLED("READY", "Tile1 -> Tile2");
      return;
    }

    Serial.println();
    Serial.println("=== LIVE SAMPLE ===");
    Serial.print("Peak1: ");
    Serial.println(peak1);
    Serial.print("Peak2: ");
    Serial.println(peak2);
    Serial.print("Interval: ");
    Serial.println(interval);

    bool authorized = classifyUser(peak1, peak2, interval);

    if (authorized) {
      Serial.println("AUTHORIZED USER");

      digitalWrite(GREEN_LED_PIN, HIGH);
      digitalWrite(RED_LED_PIN, LOW);
      digitalWrite(BUZZER_PIN, LOW);

      showOLED("AUTHORIZED", "Access granted");

      delay(8000);
    } else {
      Serial.println("UNAUTHORIZED USER");

      digitalWrite(GREEN_LED_PIN, LOW);
      digitalWrite(RED_LED_PIN, HIGH);

      showOLED("UNAUTHORIZED", "Alert active");

      longBuzzerAlert();

      delay(5000);
    }

    resetSystem();
    showOLED("READY", "Tile1 -> Tile2");
  }

  tile1WasPressed = tile1Pressed;
  tile2WasPressed = tile2Pressed;

  delay(20);
}

bool classifyUser(int livePeak1, int livePeak2, unsigned long liveInterval) {
  int diffPeak1 = abs(livePeak1 - TRAINED_PEAK1);
  int diffPeak2 = abs(livePeak2 - TRAINED_PEAK2);
  int diffInterval = abs((int)liveInterval - TRAINED_INTERVAL);

  Serial.println("=== CLASSIFICATION ===");
  Serial.print("Peak1 Diff: ");
  Serial.println(diffPeak1);
  Serial.print("Peak2 Diff: ");
  Serial.println(diffPeak2);
  Serial.print("Interval Diff: ");
  Serial.println(diffInterval);

  if (
    diffPeak1 <= PEAK1_TOLERANCE &&
    diffPeak2 <= PEAK2_TOLERANCE &&
    diffInterval <= INTERVAL_TOLERANCE
  ) {
    return true;
  }

  return false;
}

void longBuzzerAlert() {
  for (int i = 0; i < 5; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(600);
    digitalWrite(BUZZER_PIN, LOW);
    delay(250);
  }
}

void resetSystem() {
  waitingForSecondStep = false;
  firstStepTime = 0;
  lastStepTime = millis();

  peak1 = 0;
  peak2 = 0;

  tile1WasPressed = false;
  tile2WasPressed = false;

  digitalWrite(GREEN_LED_PIN, LOW);
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  Serial.println("Ready for next user...");
}

void showOLED(const char* line1, const char* line2) {
  oled.clearBuffer();

  oled.setFont(u8g2_font_7x14B_tf);
  oled.drawStr(0, 18, line1);

  oled.setFont(u8g2_font_6x12_tf);
  oled.drawStr(0, 42, line2);

  oled.sendBuffer();
}