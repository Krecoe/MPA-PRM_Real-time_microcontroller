// State classifier: reads the MPU-6050 at 50 Hz, classifies the last WINDOW samples every STEP
// samples with the model exported by train.py, and shows the smoothed state on the RGB LED.
// Serial output is only for recording and debugging; the device works without a computer.
//   raw sample lines:  t_ms,ax,ay,az,gx,gy,gz     (read by record.py)
//   prediction lines:  # P,t_ms,feature...,probability...,state
//   timing lines:      # T,microseconds for features + network
// BLE (contract in docs/BLE_PROTOCOL.md): after every decision, i.e. once per second, a notification with
//   {"state":"calm","probs":{"calm":0.91,"fast":0.05,"not_attached":0.03,"shaking":0.01}}
// The regular rate also tells the monitoring page that the board is alive.
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include "model.h"

const int SDA_PIN = 10;
const int SCL_PIN = 11;
const int LED_PIN = 48;
const unsigned long SAMPLE_PERIOD_US = 1000000UL / SAMPLE_RATE_HZ;
const int SMOOTH_DECISIONS = 3;  // average the probabilities of the last 3 windows (3 s) to avoid flicker
const char* BLE_DEVICE_NAME = "ESP32-BREATH";
const char* BLE_SERVICE_UUID = "8e7a1f20-1d0b-4f6a-9c3e-2b5c7d9e1001";  // must match monitor/app.js
const char* BLE_STATE_UUID = "8e7a1f21-1d0b-4f6a-9c3e-2b5c7d9e1001";
const float LOG_EPS = 1e-6f;  // "EPS" is already a macro in the ESP32 headers

static_assert(N_FEATURES == 6, "computeFeatures() must produce the features listed in features.py");
const int SHORT = 2 * SAMPLE_RATE_HZ;  // last 2 s, so movement vs. shaking reacts faster than breathing
const int FIRST_BREATH_BIN = 1, LAST_BREATH_BIN = 12;  // 0.125-1.5 Hz for the 400-sample window
const int LAST_LOW_BIN_SHORT = 6;  // 0-3 Hz for the 100-sample short window

Adafruit_MPU6050 mpu;
float samples[WINDOW][6];  // ring buffer of ax, ay, az, gx, gy, gz
float window[WINDOW][6];   // the same samples in time order, oldest first
int writeIndex = 0;
long samplesSeen = 0;
float recentProbabilities[SMOOTH_DECISIONS][N_CLASSES];
int decisionsSeen = 0;
BLECharacteristic* stateCharacteristic = nullptr;

class RestartAdvertisingOnDisconnect : public BLEServerCallbacks {
  // Advertising stops when a client connects; restart it so the page can reconnect
  void onDisconnect(BLEServer* server) override {
    BLEDevice::startAdvertising();
  }
};

void publishState(int bestClass, const float smoothed[N_CLASSES]) {
  char payload[160];
  int length = snprintf(payload, sizeof(payload), "{\"state\":\"%s\",\"probs\":{", CLASS_NAMES[bestClass]);
  for (int c = 0; c < N_CLASSES; c++) {
    length += snprintf(payload + length, sizeof(payload) - length, "%s\"%s\":%.2f",
                       c > 0 ? "," : "", CLASS_NAMES[c], smoothed[c]);
  }
  snprintf(payload + length, sizeof(payload) - length, "}}");
  stateCharacteristic->setValue((uint8_t*)payload, strlen(payload));
  stateCharacteristic->notify();
}

void setupBle() {
  BLEDevice::init(BLE_DEVICE_NAME);
  BLEServer* server = BLEDevice::createServer();
  server->setCallbacks(new RestartAdvertisingOnDisconnect());
  BLEService* service = server->createService(BLE_SERVICE_UUID);
  stateCharacteristic = service->createCharacteristic(
      BLE_STATE_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  stateCharacteristic->setValue("starting");
  service->start();
  BLEAdvertising* advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(BLE_SERVICE_UUID);
  advertising->setScanResponse(true);
  BLEDevice::startAdvertising();
}

void setLed(uint8_t r, uint8_t g, uint8_t b) {
  rgbLedWrite(LED_PIN, r, g, b);
}

void showState(const char* state) {
  if (strcmp(state, "calm") == 0) setLed(0, 40, 0);              // green
  else if (strcmp(state, "fast") == 0) setLed(40, 0, 0);         // red
  else if (strcmp(state, "not_attached") == 0) setLed(0, 0, 40); // blue
  else if (strcmp(state, "shaking") == 0) setLed(40, 0, 40);     // magenta
}

float hann(int i, int n) {
  return 0.5f - 0.5f * cosf(2 * PI * i / n);
}

// |X_k|^2 of the DFT; only a few bins are needed, so no FFT
float binPower(const float* signal, int n, int k) {
  double re = 0, im = 0;
  for (int i = 0; i < n; i++) {
    float phase = 2 * PI * (float)((long)k * i % n) / n;  // reduce k*i mod n to keep the float phase accurate
    re += signal[i] * cosf(phase);
    im += signal[i] * sinf(phase);
  }
  return re * re + im * im;
}

// Same as window_features() in features.py
void computeFeatures(float features[N_FEATURES]) {
  for (int i = 0; i < WINDOW; i++) memcpy(window[i], samples[(writeIndex + i) % WINDOW], sizeof(window[i]));

  double accMean[3] = {0, 0, 0};
  for (int i = 0; i < WINDOW; i++)
    for (int a = 0; a < 3; a++) accMean[a] += window[i][a];
  for (int a = 0; a < 3; a++) accMean[a] /= WINDOW;

  static float deviations[WINDOW][3];
  double covariance[3][3] = {};
  double sumSquares = 0;
  for (int i = 0; i < WINDOW; i++) {
    for (int a = 0; a < 3; a++) deviations[i][a] = window[i][a] - accMean[a];
    for (int a = 0; a < 3; a++)
      for (int b = 0; b < 3; b++) covariance[a][b] += deviations[i][a] * deviations[i][b];
  }
  for (int a = 0; a < 3; a++) {
    for (int b = 0; b < 3; b++) covariance[a][b] /= WINDOW;
    sumSquares += covariance[a][a];
  }
  float accTotalStd = sqrt(sumSquares);  // square root of the covariance trace

  // Last 2 s of the acceleration magnitude: amplitude and share of power above 3 Hz
  static float shortSignal[SHORT];
  double magSum = 0, magSumSquares = 0;
  for (int i = 0; i < SHORT; i++) {
    const float* s = window[WINDOW - SHORT + i];
    shortSignal[i] = sqrt(s[0] * s[0] + s[1] * s[1] + s[2] * s[2]);
    magSum += shortSignal[i];
    magSumSquares += (double)shortSignal[i] * shortSignal[i];
  }
  double magMean = magSum / SHORT;
  float shortMagStd = sqrt(fmax(magSumSquares / SHORT - magMean * magMean, 0.0));
  double shortEnergy = LOG_EPS;
  for (int i = 0; i < SHORT; i++) {
    shortSignal[i] = (shortSignal[i] - magMean) * hann(i, SHORT);
    shortEnergy += (double)shortSignal[i] * shortSignal[i];
  }
  // Parseval: total energy = (|X_0|^2 + 2 * sum of the other one-sided bins) / N
  double lowEnergy = binPower(shortSignal, SHORT, 0);
  for (int k = 1; k <= LAST_LOW_BIN_SHORT; k++) lowEnergy += 2 * binPower(shortSignal, SHORT, k);
  lowEnergy /= SHORT;
  float highFreqShare = 1 - lowEnergy / shortEnergy;

  // Breathing tilts the chest along one direction: find it by power iteration on the covariance
  double direction[3] = {1 / sqrt(3.0), 1 / sqrt(3.0), 1 / sqrt(3.0)};
  for (int iteration = 0; iteration < 30; iteration++) {
    double next[3] = {0, 0, 0};
    for (int a = 0; a < 3; a++)
      for (int b = 0; b < 3; b++) next[a] += covariance[a][b] * direction[b];
    double norm = sqrt(next[0] * next[0] + next[1] * next[1] + next[2] * next[2]) + LOG_EPS;
    for (int a = 0; a < 3; a++) direction[a] = next[a] / norm;
  }
  static float breathSignal[WINDOW];
  double breathEnergy = LOG_EPS;
  for (int i = 0; i < WINDOW; i++) {
    breathSignal[i] = (deviations[i][0] * direction[0] + deviations[i][1] * direction[1] +
                       deviations[i][2] * direction[2]) * hann(i, WINDOW);
    breathEnergy += (double)breathSignal[i] * breathSignal[i];
  }
  double breathPowerSum = 0;
  float bestPower = -1;
  int bestBin = FIRST_BREATH_BIN;
  for (int k = FIRST_BREATH_BIN; k <= LAST_BREATH_BIN; k++) {
    float power = binPower(breathSignal, WINDOW, k);
    breathPowerSum += power;
    if (power > bestPower) {
      bestPower = power;
      bestBin = k;
    }
  }
  float breathBandShare = 2 * breathPowerSum / WINDOW / breathEnergy;
  float breathRateBpm = bestBin * (float)SAMPLE_RATE_HZ / WINDOW * 60;

  double gyroSum = 0;
  for (int i = 0; i < WINDOW; i++)
    gyroSum += sqrt(window[i][3] * window[i][3] + window[i][4] * window[i][4] + window[i][5] * window[i][5]);
  float gyroMean = gyroSum / WINDOW;

  // Amplitudes span orders of magnitude between lying still and shaking, so use their logs
  features[0] = log(accTotalStd + LOG_EPS);
  features[1] = log(shortMagStd + LOG_EPS);
  features[2] = highFreqShare;
  features[3] = breathBandShare;
  features[4] = breathRateBpm;
  features[5] = log(gyroMean + LOG_EPS);
}

// Standardize, then the small network: hidden = relu(HIDDEN_WEIGHTS x + HIDDEN_BIAS), probabilities = softmax(OUTPUT_WEIGHTS hidden + OUTPUT_BIAS)
void predict(const float features[N_FEATURES], float probabilities[N_CLASSES]) {
  float standardized[N_FEATURES];
  for (int f = 0; f < N_FEATURES; f++) standardized[f] = (features[f] - FEATURE_MEAN[f]) / FEATURE_SCALE[f];

  float hidden[N_HIDDEN];
  for (int h = 0; h < N_HIDDEN; h++) {
    hidden[h] = HIDDEN_BIAS[h];
    for (int f = 0; f < N_FEATURES; f++) hidden[h] += HIDDEN_WEIGHTS[h][f] * standardized[f];
    hidden[h] = fmax(hidden[h], 0.0f);
  }

  float logits[N_CLASSES];
  float maxLogit = -1e30f;
  for (int c = 0; c < N_CLASSES; c++) {
    logits[c] = OUTPUT_BIAS[c];
    for (int h = 0; h < N_HIDDEN; h++) logits[c] += OUTPUT_WEIGHTS[c][h] * hidden[h];
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
  unsigned long startUs = micros();
  computeFeatures(features);
  predict(features, probabilities);
  unsigned long elapsedUs = micros() - startUs;

  memcpy(recentProbabilities[decisionsSeen % SMOOTH_DECISIONS], probabilities, sizeof(probabilities));
  decisionsSeen++;
  int count = min(decisionsSeen, SMOOTH_DECISIONS);
  float smoothed[N_CLASSES];
  int bestClass = 0;
  for (int c = 0; c < N_CLASSES; c++) {
    smoothed[c] = 0;
    for (int d = 0; d < count; d++) smoothed[c] += recentProbabilities[d][c];
    smoothed[c] /= count;
    if (smoothed[c] > smoothed[bestClass]) bestClass = c;
  }
  showState(CLASS_NAMES[bestClass]);
  publishState(bestClass, smoothed);

  Serial.printf("# P,%lu", millis());
  for (int f = 0; f < N_FEATURES; f++) Serial.printf(",%.5f", features[f]);
  for (int c = 0; c < N_CLASSES; c++) Serial.printf(",%.5f", probabilities[c]);
  Serial.printf(",%s\n", CLASS_NAMES[bestClass]);
  Serial.printf("# T,%lu\n", elapsedUs);  // time for features + network, in microseconds
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
  setupBle();
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
