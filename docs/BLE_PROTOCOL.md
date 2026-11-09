# BLE contract (firmware <-> monitoring page)

Read this before writing the BLE part of the firmware. The monitoring page in
[`/monitor`](../monitor) is already written against this contract, so firmware and page
agree without either side having to change afterwards.

## GATT

| Item | Value |
|------|-------|
| Device name | `ESP32-BREATH` |
| Service UUID | `8e7a1f20-1d0b-4f6a-9c3e-2b5c7d9e1001` |
| State characteristic UUID | `8e7a1f21-1d0b-4f6a-9c3e-2b5c7d9e1001` |
| Properties | READ, NOTIFY |
| Encoding | UTF-8 text, ASCII in practice |

The service UUID **must** be in the advertisement packet (`advertising->addServiceUUID(...)`),
because the page filters on it. Keep the device name short: a 128-bit service UUID plus a long
name does not fit into the 31-byte advertisement, and then Chrome stops listing the device.
The page also accepts a name prefix of `ESP32` or `MPA-PRM` as a fallback.

## Payload

The page accepts any of these, so ship the quick one first and upgrade later — no page change needed.

```json
{"state":"calm","probs":{"calm":0.91,"fast":0.05,"not_attached":0.03,"shaking":0.01}}
{"state":"fast","confidence":0.88}
```
```
calm
not_attached,0.74
```

State strings: `calm`, `fast`, `not_attached`, `shaking`. Also understood (mapped automatically):
`calm_breathing`, `fast_breathing`, `shake`, `hand`, `off`, `idle`.

`probs` is drawn as probability bars under the current state, so it is worth sending if the
classifier produces probabilities. It is not required.

## Send rate - important

**Notify at least once per second, even when the state did not change.**

The monitor distinguishes "board is alive and calm" from "board died / out of range" by looking at
how long it has been since the last notification (default limit: 5 s, adjustable in the UI). If the
firmware only notifies on state change, a calm person breathing quietly for 30 s looks exactly like
a dead board and the monitor raises a false "No data" alarm.

If a periodic notify is inconvenient for now, raise the "No update for longer than" field in the UI
to something large (e.g. 600) before the demo, and say so when demonstrating the connection-lost
alarm.

## Paste-ready firmware side

No new library needed - this is the BLE stack built into the Arduino ESP32 core, so nothing changes
in `platformio.ini`.

```cpp
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

const char* BLE_DEVICE_NAME = "ESP32-BREATH";
const char* BLE_SERVICE_UUID = "8e7a1f20-1d0b-4f6a-9c3e-2b5c7d9e1001";
const char* BLE_STATE_UUID = "8e7a1f21-1d0b-4f6a-9c3e-2b5c7d9e1001";
const unsigned long NOTIFY_PERIOD_MS = 1000;  // heartbeat, see "Send rate" above

BLECharacteristic* stateCharacteristic = nullptr;
unsigned long lastNotifyMs = 0;

void startBle() {
  BLEDevice::init(BLE_DEVICE_NAME);
  BLEServer* server = BLEDevice::createServer();
  BLEService* service = server->createService(BLE_SERVICE_UUID);
  stateCharacteristic = service->createCharacteristic(
      BLE_STATE_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  // The 0x2902 descriptor is what lets a central subscribe to notifications.
  stateCharacteristic->addDescriptor(new BLE2902());
  service->start();

  BLEAdvertising* advertising = server->getAdvertising();
  advertising->addServiceUUID(BLE_SERVICE_UUID);
  advertising->start();
}

void publishState(const char* state, float confidence) {
  if (millis() - lastNotifyMs < NOTIFY_PERIOD_MS) return;
  lastNotifyMs = millis();
  char payload[96];
  snprintf(payload, sizeof(payload), "{\"state\":\"%s\",\"confidence\":%.2f}", state, confidence);
  stateCharacteristic->setValue((uint8_t*)payload, strlen(payload));
  stateCharacteristic->notify();
}
```

Call `startBle()` once in `setup()`, and `publishState(smoothedState, topProbability)` from
`loop()` - the rate limiting is inside, so calling it every loop pass is fine.

`BLE2902` is included with the core. If the linker cannot find it, the platform is building without
Bluetooth support; say so early rather than losing an hour to it.

## Check BLE before touching the web page

Exactly as the assignment hints: with **nRF Connect** on an Android phone, connect to
`ESP32-BREATH`, expand our service, subscribe to the state characteristic notifications and watch
the values arrive as UTF-8 text. Once that works, the web page is only a consumer of the same data.

## RAM when BLE, the Wi-Fi hotspot and the classifier run together

The demo needs all three at once (parts 3 and 4 are shown together). The stock Arduino BLE stack is
fairly large; if allocation starts failing once the Wi-Fi access point and HTTP server are up,
switch to NimBLE, which has nearly the same API surface and much lower footprint:

```ini
lib_deps =
    adafruit/Adafruit MPU6050
    h2zero/NimBLE-Arduino
```

Symptoms to watch for: `wifi: wifi_softap_dhcps start failed`, `bmp5_alloc`, reboots with
`Guest startup failed`, or the BLE task being deleted. Report which one you see, because each points
somewhere different.

## Testing the page without the board

`monitor/index.html?simulate=scenario&fast=4&notAttached=5` drives the whole page from a fake
board (timeline, both duration alarms) so the page can be demonstrated and rehearsed on a laptop.
`?simulate=fast&fast=3&drop=6` additionally raises the connection-lost alarm after 6 s.
