// Sensor and LED test: reads the MPU-6050 at 50 Hz, prints CSV over serial,
// and cycles the RGB LED through the four state colours.
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>

const int SDA_PIN = 10;
const int SCL_PIN = 11;
const int LED_PIN = 48;  // on-board WS2812 on most ESP32-S3 DevKitC boards (38 on some revisions)
const unsigned long SAMPLE_PERIOD_US = 20000;  // 50 Hz
const unsigned long LED_STEP_MS = 2000;

Adafruit_MPU6050 mpu;

void setLed(uint8_t r, uint8_t g, uint8_t b) {
  neopixelWrite(LED_PIN, r, g, b);
}

void scanI2C() {
  Serial.print("# I2C devices:");
  for (uint8_t address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) Serial.printf(" 0x%02X", address);
  }
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Wire.begin(SDA_PIN, SCL_PIN);
  scanI2C();

  if (!mpu.begin(0x68, &Wire)) {
    Serial.println("# ERROR: MPU-6050 not found");
    while (true) {
      setLed(30, 0, 0);
      delay(200);
      setLed(0, 0, 0);
      delay(200);
    }
  }
  // +-4 g so that shaking does not clip; 21 Hz low-pass stays below the 25 Hz Nyquist limit
  mpu.setAccelerometerRange(MPU6050_RANGE_4_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  Serial.println("t_ms,ax,ay,az,gx,gy,gz");
}

void loop() {
  static unsigned long nextSampleUs = micros();
  static unsigned long lastLedChangeMs = 0;
  static int colourIndex = 0;

  // green, red, blue, magenta = calm, fast, not attached, shaking
  const uint8_t colours[4][3] = {{0, 40, 0}, {40, 0, 0}, {0, 0, 40}, {40, 0, 40}};

  if (millis() - lastLedChangeMs >= LED_STEP_MS) {
    lastLedChangeMs = millis();
    setLed(colours[colourIndex][0], colours[colourIndex][1], colours[colourIndex][2]);
    colourIndex = (colourIndex + 1) % 4;
  }

  if ((long)(micros() - nextSampleUs) >= 0) {
    nextSampleUs += SAMPLE_PERIOD_US;
    sensors_event_t accel, gyro, temp;
    mpu.getEvent(&accel, &gyro, &temp);
    Serial.printf("%lu,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f\n", millis(),
                  accel.acceleration.x, accel.acceleration.y, accel.acceleration.z,
                  gyro.gyro.x, gyro.gyro.y, gyro.gyro.z);
  }
}
