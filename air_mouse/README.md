# Air mouse (optional add-on)

A separate firmware that turns the board into a Bluetooth mouse:
- **tilt** the breadboard to move the pointer (a joystick: the more you tilt, the faster it moves),
- **tap** the board with a finger for a **left click**.

Everything runs on the ESP32-S3. The board is a standard BLE HID mouse, so the computer needs no
extra software, and USB only powers it.

## How it works
- The MPU-6050 is read at 100 Hz (range ±8 g, 260 Hz bandwidth so short taps are not smoothed away).
- At reset, the board averages 1 s of acceleration as the **rest pose** (LED yellow). Hold it still the way you will hold it.
- **Pointer:** acceleration is low-pass filtered (about 50 ms). The change of gravity along the
  board's X and Y axes compared with the rest pose is the tilt. Tilt under 0.08 g (about 5°) is ignored;
  beyond that, the pointer speed grows linearly (40 px per sample per g). Sub-pixel movement is carried over
  so small tilts still move slowly.
- **Tap:** a tap is a spike of a few ms, so the distance between the raw sample and the low-pass value jumps.
  Above 0.6 g it is a click (button down, 20 ms, button up). For 300 ms after a tap, further spikes and the
  pointer are ignored, so the ringing of one tap is not a double click and the tap does not jerk the pointer.
  Spike samples are kept out of the low-pass filter for the same reason.

## LED
| LED    | Meaning                              |
|--------|--------------------------------------|
| yellow | calibrating the rest pose (1 s)      |
| blue   | waiting for the computer to connect  |
| green  | connected                            |
| white flash | click                           |
| red    | MPU-6050 not found                   |

## Build, flash, pair
```
cd air_mouse
..\.venv\Scripts\pio.exe run -t upload
```
Then on Windows: Settings → Bluetooth & devices → Add device → Bluetooth → **ESP32-AirMouse**.
To go back to the breathing classifier, flash `firmware/` again (`cd firmware`, same command).
If Windows does not reconnect after reflashing, remove the device in Bluetooth settings and pair again.

## Tuning
The constants are at the top of `src/main.cpp`. `pio device monitor` prints every tap with its size
(`# tap,1.35`) and once per second the tilt and the largest deviation seen (`# -0.02,0.10,0.07`), which
helps to set `TAP_THRESHOLD_G` above normal hand movement and below a tap.
If the pointer moves the wrong way, flip `X_SIGN` or `Y_SIGN`.
