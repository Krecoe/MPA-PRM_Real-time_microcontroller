// Air mouse: tilt the board to move the mouse pointer, tap it for a left click.
// The board is a Bluetooth (BLE HID) mouse, so the computer needs no extra software and USB only powers it.
// Serial output is only for tuning: "# tap,<g>" for every click and once per second
//   "# tilt_x,tilt_y,largest deviation" (all in g).
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEHIDDevice.h>
#include <BLESecurity.h>

const int SDA_PIN = 10;
const int SCL_PIN = 11;
const int LED_PIN = 48;
const int SAMPLE_RATE_HZ = 100;
const unsigned long SAMPLE_PERIOD_US = 1000000UL / SAMPLE_RATE_HZ;
const float STANDARD_GRAVITY = 9.80665f;
const char* BLE_DEVICE_NAME = "ESP32-AirMouse";

// Pointer: the tilt (change of gravity along the board plane from the rest pose) sets the pointer speed
const float DEAD_ZONE_G = 0.08f;     // about 5 degrees of tilt is ignored, so the pointer stays put when held level
const float SPEED_PX_PER_G = 40.0f;  // pixels per sample for each g of tilt beyond the dead zone
const float SMOOTHING = 0.2f;        // low-pass weight of the newest sample, about 50 ms time constant at 100 Hz
// Sensor X points right and Y away from the USB connector (printed on the HW-123). Tilting the right edge
// down makes ax negative, tilting the far edge down makes ay negative. Flip a sign if a direction feels wrong.
const int X_SIGN = -1;
const int Y_SIGN = 1;

// Tap: a short acceleration spike that a hand movement cannot produce
const float TAP_THRESHOLD_G = 0.6f;
const unsigned long TAP_LOCKOUT_MS = 300;  // a tap keeps ringing; ignore it, and hold the pointer still, until it fades
const int CALIBRATION_SAMPLES = SAMPLE_RATE_HZ;  // 1 s at rest after reset

// Standard 3-button mouse with a wheel; the report is buttons, x, y, wheel
const uint8_t MOUSE_REPORT_DESCRIPTOR[] = {
  0x05, 0x01,  // Usage Page (Generic Desktop)
  0x09, 0x02,  // Usage (Mouse)
  0xA1, 0x01,  // Collection (Application)
  0x85, 0x01,  //   Report ID (1)
  0x09, 0x01,  //   Usage (Pointer)
  0xA1, 0x00,  //   Collection (Physical)
  0x05, 0x09,  //     Usage Page (Button)
  0x19, 0x01,  //     Usage Minimum (1)
  0x29, 0x03,  //     Usage Maximum (3)
  0x15, 0x00,  //     Logical Minimum (0)
  0x25, 0x01,  //     Logical Maximum (1)
  0x95, 0x03,  //     Report Count (3)
  0x75, 0x01,  //     Report Size (1)
  0x81, 0x02,  //     Input (Data, Variable, Absolute)
  0x95, 0x01,  //     Report Count (1)
  0x75, 0x05,  //     Report Size (5)
  0x81, 0x03,  //     Input (Constant): padding to a full byte
  0x05, 0x01,  //     Usage Page (Generic Desktop)
  0x09, 0x30,  //     Usage (X)
  0x09, 0x31,  //     Usage (Y)
  0x09, 0x38,  //     Usage (Wheel)
  0x15, 0x81,  //     Logical Minimum (-127)
  0x25, 0x7F,  //     Logical Maximum (127)
  0x75, 0x08,  //     Report Size (8)
  0x95, 0x03,  //     Report Count (3)
  0x81, 0x06,  //     Input (Data, Variable, Relative)
  0xC0,        //   End Collection
  0xC0         // End Collection
};
const uint8_t LEFT_BUTTON = 0x01;

Adafruit_MPU6050 mpu;
BLECharacteristic* mouseReport = nullptr;
bool connected = false;
float restGravity[3] = {0, 0, 0};
float smoothed[3] = {0, 0, 0};
float leftoverX = 0, leftoverY = 0;  // sub-pixel movement carried to the next report, so small tilts still move
unsigned long lastTapMs = 0;
float largestDeviation = 0;
unsigned long lastPrintMs = 0;

class TrackConnection : public BLEServerCallbacks {
  void onConnect(BLEServer* server) override {
    connected = true;
  }
  // Advertising stops when a computer connects; restart it so the computer can reconnect
  void onDisconnect(BLEServer* server) override {
    connected = false;
    BLEDevice::startAdvertising();
  }
};

void setLed(uint8_t r, uint8_t g, uint8_t b) {
  rgbLedWrite(LED_PIN, r, g, b);
}

void showConnection() {
  if (connected) setLed(0, 40, 0);  // green: connected
  else setLed(0, 0, 40);            // blue: waiting for the computer
}

void sendMouse(uint8_t buttons, int dx, int dy) {
  uint8_t report[4] = {buttons, (uint8_t)(int8_t)dx, (uint8_t)(int8_t)dy, 0};
  mouseReport->setValue(report, sizeof(report));
  mouseReport->notify();
}

void setupBleMouse() {
  BLEDevice::init(BLE_DEVICE_NAME);
  // "Just works" pairing with bonding, like an ordinary Bluetooth mouse
  BLESecurity* security = new BLESecurity();
  security->setCapability(ESP_IO_CAP_NONE);
  security->setAuthenticationMode(true, false, true);

  BLEServer* server = BLEDevice::createServer();
  server->setCallbacks(new TrackConnection());
  BLEHIDDevice* hid = new BLEHIDDevice(server);
  hid->manufacturer()->setValue("MPA-PRM");
  hid->pnp(0x02, 0xe502, 0xa111, 0x0210);
  hid->hidInfo(0x00, 0x01);
  hid->reportMap((uint8_t*)MOUSE_REPORT_DESCRIPTOR, sizeof(MOUSE_REPORT_DESCRIPTOR));
  mouseReport = hid->inputReport(1);
  hid->setBatteryLevel(100);
  hid->startServices();

  BLEAdvertising* advertising = BLEDevice::getAdvertising();
  advertising->setAppearance(0x03C2);  // HID mouse
  advertising->addServiceUUID(hid->hidService()->getUUID());
  advertising->setScanResponse(true);
  BLEDevice::startAdvertising();
}

void readAcceleration(float acceleration[3]) {
  sensors_event_t accel, gyro, temp;
  mpu.getEvent(&accel, &gyro, &temp);
  acceleration[0] = accel.acceleration.x / STANDARD_GRAVITY;
  acceleration[1] = accel.acceleration.y / STANDARD_GRAVITY;
  acceleration[2] = accel.acceleration.z / STANDARD_GRAVITY;
}

// Tilt beyond the dead zone -> pointer speed in pixels per sample
float pointerSpeed(float tilt) {
  float beyond = fabsf(tilt) - DEAD_ZONE_G;
  if (beyond <= 0) return 0;
  return copysignf(beyond * SPEED_PX_PER_G, tilt);
}

int takeWholePixels(float& leftover) {
  int pixels = constrain((int)leftover, -127, 127);
  leftover -= pixels;
  return pixels;
}

void setup() {
  Serial.begin(115200);
  setLed(40, 40, 0);  // yellow: calibrating
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);
  while (!mpu.begin()) {
    Serial.println("# MPU-6050 not found, check the wiring");
    setLed(40, 0, 0);
    delay(1000);
  }
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);  // taps can exceed the default 2 g range
  mpu.setFilterBandwidth(MPU6050_BAND_260_HZ);   // a tap lasts only a few ms; a lower bandwidth would smear it out

  // The rest pose is however the board is held during the first second
  for (int i = 0; i < CALIBRATION_SAMPLES; i++) {
    float acceleration[3];
    readAcceleration(acceleration);
    for (int a = 0; a < 3; a++) restGravity[a] += acceleration[a] / CALIBRATION_SAMPLES;
    delayMicroseconds(SAMPLE_PERIOD_US);
  }
  memcpy(smoothed, restGravity, sizeof(smoothed));
  Serial.printf("# rest pose %.2f %.2f %.2f g\n", restGravity[0], restGravity[1], restGravity[2]);

  setupBleMouse();
  showConnection();
  Serial.printf("# pair %s in the Bluetooth settings of the computer\n", BLE_DEVICE_NAME);
}

void loop() {
  static unsigned long nextSampleUs = micros();
  if ((long)(micros() - nextSampleUs) < 0) return;
  nextSampleUs += SAMPLE_PERIOD_US;

  float acceleration[3];
  readAcceleration(acceleration);
  float deviationSquared = 0;
  for (int a = 0; a < 3; a++) deviationSquared += sq(acceleration[a] - smoothed[a]);
  float deviation = sqrtf(deviationSquared);
  largestDeviation = max(largestDeviation, deviation);
  unsigned long now = millis();
  bool tapRinging = now - lastTapMs < TAP_LOCKOUT_MS;

  if (!tapRinging && deviation > TAP_THRESHOLD_G) {
    lastTapMs = now;
    Serial.printf("# tap,%.2f\n", deviation);
    if (connected) {
      setLed(40, 40, 40);  // white flash
      sendMouse(LEFT_BUTTON, 0, 0);
      delay(20);  // the computer must see the button down before it goes up
      sendMouse(0, 0, 0);
    }
  } else if (!tapRinging) {
    // Spikes are kept out of the low-pass filter, so a tap does not jerk the pointer
    for (int a = 0; a < 3; a++) smoothed[a] += SMOOTHING * (acceleration[a] - smoothed[a]);
    if (connected) {
      showConnection();
      leftoverX += X_SIGN * pointerSpeed(smoothed[0] - restGravity[0]);
      leftoverY += Y_SIGN * pointerSpeed(smoothed[1] - restGravity[1]);
      int dx = takeWholePixels(leftoverX);
      int dy = takeWholePixels(leftoverY);
      if (dx != 0 || dy != 0) sendMouse(0, dx, dy);
    }
  }

  if (now - lastPrintMs >= 1000) {
    lastPrintMs = now;
    Serial.printf("# %.2f,%.2f,%.2f\n", smoothed[0] - restGravity[0], smoothed[1] - restGravity[1], largestDeviation);
    largestDeviation = 0;
    if (!connected) showConnection();
  }
}
