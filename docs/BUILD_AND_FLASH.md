# Build and flash

## Prerequisites

- PlatformIO Core (the `pio` command). We installed it into the project's virtual environment with
  `python -m pip install -r requirements.txt` (PlatformIO 6.2.0) and call it as `../.venv/Scripts/pio`
  from `firmware/`. With the PlatformIO extension in VS Code it is already there.
- Nothing else has to be installed by hand: the toolchain, the Arduino framework and the libraries
  below are fetched by PlatformIO on the first build.
- Board: ESP32-S3 N16R8 (16 MB flash, 8 MB octal PSRAM). Our devkit's USB-C port goes through a
  CH343 USB-serial chip (`USB VID:PID=1A86:55D3`). If the COM port does not appear, install the CH343
  driver.
- The first build downloads the ESP32 toolchain and framework (several hundred MB); it took about
  9 minutes for us. Later builds take under 2 minutes.

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
- `upload_port` / `monitor_port` are COM7 on our laptop. Find yours with `pio device list` and change
  both lines.
- Library used for the sensor: Adafruit MPU6050. BLE, Wi-Fi and the web server (`WebServer`) are
  built into the Arduino ESP32 core, so they add no dependency (see [BLE_PROTOCOL.md](BLE_PROTOCOL.md)).
  With BLE, the Wi-Fi hotspot and the web server together the build uses 23.3 % of the static RAM and
  41.2 % of the app flash, so the NimBLE fallback was not needed.

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

After flashing, the serial monitor shows

```
# Wi-Fi hotspot ESP32-BREATH, page at http://192.168.4.1/
t_ms,ax,ay,az,gx,gy,gz
...                                   raw samples at 50 Hz (what record.py saves)
# P,9627,<6 features>,<4 probabilities>,not_attached     one decision per second
# T,43370                             microseconds for features + network
```

The LED is dim white for the first 8 s (until the first window is full), then shows the state.
If the sensor is not found, the firmware prints `# ERROR: MPU-6050 not found` and the LED blinks red:
check the SDA/SCL swap first (SDA = GPIO10, SCL = GPIO11) and that VCC is 3.3 V, not 5 V.

The first commit of the firmware (`1789834`, "Add sensor and LED test firmware") is a minimal sensor
and LED test with an I2C scan; it is useful to check new wiring.

## Flashing without the correct COM port

`pio run -t upload` can fail because the monitor is holding the port. Close the other monitor, or
hold BOOT while pressing RESET to enter the bootloader and retry.

## GitHub Pages for the monitoring page

Web Bluetooth needs https, so the page must be published:

- Repository → **Settings → Pages → Build and deployment → Source: Deploy from a branch**
- Currently: branch `Seba_branch`, folder `/` (the repository root). The root `index.html` links to
  `monitor/`, so the monitor is at <https://krecoe.github.io/MPA-PRM_Real-time_microcontroller/monitor/>.
- `main` holds the same `monitor/` folder; switch the source to `main` if the page changes there.
- After saving, wait about a minute, then open the https URL in Chrome or Edge.

The repository is public, which is what makes this possible at all — a private repository cannot
serve Pages.
