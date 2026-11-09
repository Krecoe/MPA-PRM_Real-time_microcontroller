# Build and flash

## Prerequisites

- PlatformIO Core (the `pio` command). With the PlatformIO extension in VS Code it is already there;
  otherwise <https://docs.platformio.org/en/latest/installation.html>.
- Nothing else has to be installed by hand: the toolchain, the Arduino framework and the libraries
  below are fetched by PlatformIO on the first build.
- Board: ESP32-S3 N16R8 (16 MB flash, 8 MB octal PSRAM). Driver: if the COM port does not appear,
  install the USB-UART bridge driver (CH343 / CP210x / native USB-CDC depending on the devkit).

## Configuration

`firmware/platformio.ini` as used:

```ini
[env:esp32s3]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
board_upload.flash_size = 16MB
board_build.arduino.memory_type = qio_opi   ; needed for the octal PSRAM on N16R8
build_flags = -DBOARD_HAS_PSRAM
monitor_speed = 115200
upload_port = COM7
monitor_port = COM7
lib_deps =
    adafruit/Adafruit MPU6050
```

- `qio_opi` is what makes the 8 MB PSRAM visible; without it the board boots but has far less RAM.
- `upload_port` is TODO(fill): confirm the COM port on the machine doing the demo.
  Find it with `pio device list`.
- Library used for the sensor: Adafruit MPU6050. BLE uses the stack built into the Arduino ESP32
  core, so it adds no dependency (see [BLE_PROTOCOL.md](BLE_PROTOCOL.md)).
  If RAM gets tight once Wi-Fi and BLE are both up, NimBLE is listed there as the fallback.

## Commands

Run from the `firmware/` directory.

```bash
pio run                 # build
pio run -t upload       # build and flash
pio device monitor      # serial console at 115200, Ctrl+] to quit
pio run -t upload -t monitor   # both
pio device list         # which COM ports exist
pio run -t clean        # clean build
```

Arduino IDE also works (open `firmware/src/main.cpp`, select the board and port), but PlatformIO
keeps the exact build configuration in the repository, which is what makes this rebuildable.

## Expected output

After the test/sensor build, the serial monitor shows an I2C scan listing `0x68`, then CSV lines at
50 Hz, and the on-board LED cycles green → red → blue → magenta every 2 s. If the scan shows no
device, check the SDA/SCL swap first (SDA = GPIO10, SCL = GPIO11) and that VCC is 3.3 V, not 5 V.

## Flashing without the correct COM port

`pio run -t upload` can fail because the monitor is holding the port. Close the other monitor, or
hold BOOT while pressing RESET to enter the bootloader and retry.

## GitHub Pages for the monitoring page

Web Bluetooth needs https, so `monitor/` must be published:

- Repository → **Settings → Pages → Build and deployment → Source: Deploy from a branch**
- Branch: `Seba_branch` (or `main`, whichever holds the finished page), folder **`/monitor`**
- Save, wait about a minute, open the printed https URL in Chrome or Edge.

The repository is public, which is what makes this possible at all — a private repository cannot
serve Pages.
