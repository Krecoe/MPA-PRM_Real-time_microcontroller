# Real-time AI on a microcontroller

An ESP32-S3 with an MPU-6050 sensor recognizes four states (calm breathing, fast breathing,
not attached, shaking) and shows them on its LED. Everything runs on the board.

**Group:** Kristof Krupa, Sebastian Černý

**Monitoring page (BLE):** <https://krecoe.github.io/MPA-PRM_Real-time_microcontroller/monitor/>

**Documentation:** [docs/documentation.html](docs/documentation.html) (open it in a browser)

**Own extension:** [air mouse](air_mouse/README.md): a separate firmware that turns the board into a
Bluetooth mouse; tilt the board to move the pointer, tap it to click.

![Wiring](docs/wiring.jpg)

| MPU-6050 (HW-123) | ESP32-S3 |
|-------------------|----------|
| VCC | 3.3 V |
| GND | GND |
| SDA | GPIO10 |
| SCL | GPIO11 |
