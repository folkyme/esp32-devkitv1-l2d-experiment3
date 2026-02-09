#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ================= Hardware =================
#define TdsSensorPin 33
#define VREF 3.3f
#define ADC_RES 4096.0f

// ================= Timing ==================
#define SAMPLE_INTERVAL_MS    40
#define DISPLAY_INTERVAL_MS   600

// ================= Filter ==================
#define SCOUNT     15          // Median filter (เร็ว)
#define EMA_ALPHA  0.45f       // EMA (นิ่ง + เร็ว)

// ================= Calibration =============
#define TDS_FACTOR 1.8f       // ปรับได้ (1.5 – 2.5)

// ================= Buffers =================
int analogBuffer[SCOUNT];
int analogBufferTemp[SCOUNT];
int bufIndex = 0;
bool bufferFilled = false;

// ================= Variables ===============
float temperature = 25.0f;
float avgVoltage = 0;
float tdsRaw = 0;
float tdsValue = 0;

// ================= LCD =====================
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ================= Function ================
int getMedianNum(int bArray[], int len);

// ==========================================

void setup() {
  Serial.begin(115200);

  analogReadResolution(12);
  analogSetPinAttenuation(TdsSensorPin, ADC_11db);

  Wire.begin();
  lcd.init();
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print("TDS Meter");
  lcd.setCursor(0, 1);
  lcd.print("Starting...");
  delay(1200);
  lcd.clear();

  for (int i = 0; i < SCOUNT; i++) analogBuffer[i] = 0;
}

// ==========================================

void loop() {
  static unsigned long sampleTimer = 0;
  static unsigned long displayTimer = 0;
  static float tdsEMA = 0;

  // -------- ADC Sampling --------
  if (millis() - sampleTimer >= SAMPLE_INTERVAL_MS) {
    sampleTimer = millis();
    analogBuffer[bufIndex++] = analogRead(TdsSensorPin);
    if (bufIndex >= SCOUNT) {
      bufIndex = 0;
      bufferFilled = true;
    }
  }

  // -------- Processing --------
  if (millis() - displayTimer >= DISPLAY_INTERVAL_MS) {
    displayTimer = millis();
    if (!bufferFilled) return;

    for (int i = 0; i < SCOUNT; i++)
      analogBufferTemp[i] = analogBuffer[i];

    avgVoltage =
      getMedianNum(analogBufferTemp, SCOUNT) * VREF / ADC_RES;

    float compensation =
      1.0f + 0.02f * (temperature - 25.0f);
    float cv = avgVoltage / compensation;

    if (cv < 0) cv = 0;
    if (cv > 2.3f) cv = 2.3f;

    // ---- TDS Polynomial ----
    tdsRaw =
      (133.42f * cv * cv * cv
      -255.86f * cv * cv
      +857.39f * cv) * TDS_FACTOR;

    // ---- EMA Filter ----
    if (tdsEMA == 0) tdsEMA = tdsRaw;
    tdsEMA = EMA_ALPHA * tdsRaw + (1 - EMA_ALPHA) * tdsEMA;
    tdsValue = tdsEMA;

    // ---- Serial ----
    Serial.print("V: ");
    Serial.print(avgVoltage, 3);
    Serial.print(" | TDS: ");
    Serial.print(tdsValue, 0);
    Serial.println(" ppm");

    // ---- LCD ----
    lcd.setCursor(0, 0);
    lcd.print("TDS:        ");
    lcd.setCursor(5, 0);
    lcd.print(tdsValue, 0);
    lcd.print("ppm");

    lcd.setCursor(0, 1);
    lcd.print("V:      ");
    lcd.setCursor(2, 1);
    lcd.print(avgVoltage, 2);
  }
}

// ==========================================

int getMedianNum(int bArray[], int len) {
  for (int j = 0; j < len - 1; j++) {
    for (int i = 0; i < len - j - 1; i++) {
      if (bArray[i] > bArray[i + 1]) {
        int t = bArray[i];
        bArray[i] = bArray[i + 1];
        bArray[i + 1] = t;
      }
    }
  }
  return bArray[len / 2];
}
