// State classifier: reads the MPU-6050 at 50 Hz, classifies the last WINDOW samples every STEP
// samples with the model exported by train.py, and shows the smoothed state on the RGB LED.
// Serial output is only for recording and debugging; the device works without a computer.
//   raw sample lines:  t_ms,ax,ay,az,gx,gy,gz     (read by record.py)
//   prediction lines:  # P,t_ms,feature...,probability...,state
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include "model.h"

const int SDA_PIN = 10;
const int SCL_PIN = 11;
const int LED_PIN = 48;
const unsigned long SAMPLE_PERIOD_US = 1000000UL / SAMPLE_RATE_HZ;
const int SMOOTH_DECISIONS = 3;  // average the probabilities of the last 3 windows (3 s) to avoid flicker
const float LOG_EPS = 1e-6f;  // "EPS" is already a macro in the ESP32 headers

static_assert(N_FEATURES == 1, "computeFeatures() must produce the features listed in train.py");

Adafruit_MPU6050 mpu;
float samples[WINDOW][6];  // ring buffer of ax, ay, az, gx, gy, gz
int writeIndex = 0;
long samplesSeen = 0;
float recentProbabilities[SMOOTH_DECISIONS][N_CLASSES];
int decisionsSeen = 0;

void setLed(uint8_t r, uint8_t g, uint8_t b) {
  rgbLedWrite(LED_PIN, r, g, b);
}

void showState(const char* state) {
  if (strcmp(state, "calm") == 0) setLed(0, 40, 0);              // green
  else if (strcmp(state, "fast") == 0) setLed(40, 0, 0);         // red
  else if (strcmp(state, "not_attached") == 0) setLed(0, 0, 40); // blue
  else if (strcmp(state, "shaking") == 0) setLed(40, 0, 40);     // magenta
}

// Same as window_features() in train.py
void computeFeatures(float features[N_FEATURES]) {
  double sum = 0, sumSquares = 0;
  for (int i = 0; i < WINDOW; i++) {
    double accMag = sqrt(samples[i][0] * samples[i][0] + samples[i][1] * samples[i][1] +
                         samples[i][2] * samples[i][2]);
    sum += accMag;
    sumSquares += accMag * accMag;
  }
  double mean = sum / WINDOW;
  double variance = sumSquares / WINDOW - mean * mean;  // population variance, as numpy std()
  features[0] = log(sqrt(fmax(variance, 0.0)) + LOG_EPS);
}

// Standardize, apply the linear model and softmax
void predict(const float features[N_FEATURES], float probabilities[N_CLASSES]) {
  float logits[N_CLASSES];
  float maxLogit = -1e30f;
  for (int c = 0; c < N_CLASSES; c++) {
    logits[c] = BIAS[c];
    for (int f = 0; f < N_FEATURES; f++) {
      logits[c] += WEIGHTS[c][f] * (features[f] - FEATURE_MEAN[f]) / FEATURE_SCALE[f];
    }
    maxLogit = fmax(maxLogit, logits[c]);
  }
  float total = 0;
  for (int c = 0; c < N_CLASSES; c++) {
    probabilities[c] = exp(logits[c] - maxLogit);  // subtract the max for numerical stability
    total += probabilities[c];
  }
  for (int c = 0; c < N_CLASSES; c++) probabilities[c] /= total;
}

void classifyWindow() {
  float features[N_FEATURES];
  float probabilities[N_CLASSES];
  computeFeatures(features);
  predict(features, probabilities);

  memcpy(recentProbabilities[decisionsSeen % SMOOTH_DECISIONS], probabilities, sizeof(probabilities));
  decisionsSeen++;
  int count = min(decisionsSeen, SMOOTH_DECISIONS);
  int bestClass = 0;
  float bestProbability = -1;
  for (int c = 0; c < N_CLASSES; c++) {
    float average = 0;
    for (int d = 0; d < count; d++) average += recentProbabilities[d][c];
    average /= count;
    if (average > bestProbability) {
      bestProbability = average;
      bestClass = c;
    }
  }
  showState(CLASS_NAMES[bestClass]);

  Serial.printf("# P,%lu", millis());
  for (int f = 0; f < N_FEATURES; f++) Serial.printf(",%.5f", features[f]);
  for (int c = 0; c < N_CLASSES; c++) Serial.printf(",%.5f", probabilities[c]);
  Serial.printf(",%s\n", CLASS_NAMES[bestClass]);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Wire.begin(SDA_PIN, SCL_PIN);
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
  setLed(10, 10, 10);  // dim white until the first window is full
  Serial.println("t_ms,ax,ay,az,gx,gy,gz");
}

void loop() {
  static unsigned long nextSampleUs = micros();
  if ((long)(micros() - nextSampleUs) < 0) return;
  nextSampleUs += SAMPLE_PERIOD_US;

  sensors_event_t accel, gyro, temp;
  mpu.getEvent(&accel, &gyro, &temp);
  float sample[6] = {accel.acceleration.x, accel.acceleration.y, accel.acceleration.z,
                     gyro.gyro.x, gyro.gyro.y, gyro.gyro.z};
  memcpy(samples[writeIndex], sample, sizeof(sample));
  writeIndex = (writeIndex + 1) % WINDOW;
  samplesSeen++;
  Serial.printf("%lu,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f\n", millis(),
                sample[0], sample[1], sample[2], sample[3], sample[4], sample[5]);

  if (samplesSeen >= WINDOW && samplesSeen % STEP == 0) classifyWindow();
}
