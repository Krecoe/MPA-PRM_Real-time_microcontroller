# Air mouse (own extension)

A separate firmware that turns the board into a Bluetooth mouse:
- **tilt** the breadboard to move the pointer (like a joystick: more tilt = faster),
- **tap** the board with a finger for a **left click**.

Everything runs on the ESP32-S3. The board is a standard BLE HID mouse, so the computer needs no
extra software, and USB only powers it.

## How to use it
1. Flash it (below). Hold the board still for the first second after reset: that pose becomes "level" (LED yellow).
2. On Windows: Settings → Bluetooth & devices → Add device → Bluetooth → **ESP32-AirMouse**.
3. Hold the breadboard with the USB cable toward you. Tilt the right edge down → the pointer moves right;
   tilt the far edge down → the pointer moves up. Tap the board → left click (LED flashes white).

| LED         | Meaning                             |
|-------------|-------------------------------------|
| yellow      | measuring the level pose (1 s)      |
| blue        | waiting for the computer to connect |
| green       | connected                           |
| white flash | click                               |
| red         | MPU-6050 not found                  |

## How it works
- The MPU-6050 is read at 100 Hz, range ±8 g, 260 Hz bandwidth (so a tap of a few ms is not smoothed away).
- **Pointer:** the acceleration is low-pass filtered (about 50 ms). Its change along the board's X and Y axes
  compared with the level pose is the tilt. Tilt below 0.08 g (about 5°) is ignored, beyond that the speed
  grows linearly (40 px per sample per g). Leftover sub-pixel movement is carried to the next sample, so
  small tilts still move the pointer slowly.
- **Tap:** a tap is a short spike, so the raw sample jumps far from the low-pass value. A jump above 0.6 g is a
  click (button down, 20 ms, button up). For 300 ms after a tap, the ringing is ignored and the pointer is held
  still, so one tap is one click and does not jerk the pointer. Spike samples are kept out of the low-pass filter.
- At rest the jump between samples was 0.01–0.03 g (serial log), well below the 0.6 g tap threshold.

## Build and flash
```
cd air_mouse
..\.venv\Scripts\pio.exe run -t upload
```
This replaces the breathing classifier. To go back, run the same command in `firmware/`.

## Tuning and troubleshooting
- Constants are at the top of `src/main.cpp`: `TAP_THRESHOLD_G`, `SPEED_PX_PER_G`, `DEAD_ZONE_G`, `X_SIGN`, `Y_SIGN`.
- `..\.venv\Scripts\pio.exe device monitor` prints every tap with its size (`# tap,1.35`) and once per second
  the tilt and the largest jump (`# -0.02,0.10,0.07`).
- After reflashing, Windows may keep the old connection: if the LED stays blue, remove ESP32-AirMouse in the
  Bluetooth settings and pair again. Test only when the LED is green.
- If the serial log repeats `No bootable app partitions`, an upload was interrupted. Flash again, with nothing
  else (serial monitor, a second upload) using COM7 at the same time.
