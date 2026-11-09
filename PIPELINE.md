# Pipeline

Items marked **(open)** still need a decision. See "Open questions" at the bottom.

## 0. Hardware
- ESP32-S3 N16R8 and MPU-6050 (HW-123) on I2C: SDA = GPIO10, SCL = GPIO11, VCC = 3.3 V, address 0x68.
- On-board RGB LED: addressable WS2812 on **GPIO48** (verified on our board).
- Tip from the task: move the whole breadboard, and press it against the chest for breathing.

## 1. Sensor check (firmware)
- Toolchain: **Arduino framework with PlatformIO**.
- Read the accelerometer and gyroscope (6 axes) at a fixed **50 Hz**.
- Print the values over serial to confirm the wiring. Test the LED colours.

## 2. Data recording (laptop + board)
- Recording firmware streams the raw samples over USB serial. USB data is allowed for collecting data, just not during the demo.
- A Python script saves each session to `data/<label>_<session>.csv`. Data files are never committed.
- Classes: `calm`, `fast`, `not_attached` (held in the hand and lying still), `shaking`. Part 1 uses only `not_attached` (slow hand movement) and `shaking`.
- Record several different sessions per class (different people, postures and positions).

## 3. Features (Python, mirrored later in C++)
- Sliding windows of **8 s (400 samples), new decision every 1 s**. Breathing is slow (about 0.2–1 Hz), so windows must cover several breaths. Settings live in `features.py`.
- Simple per-window features for each axis and for the magnitude: mean, standard deviation, range, energy, and dominant frequency / band power.
- Log-transform positive amplitude features (tip from the task).
- The features must be computed **identically** on the board, so keep them simple.

### Part 1 result (movement vs. shaking, 3 + 3 sessions of 60 s, leave-one-session-out)
| Features | Mean accuracy | Worst session |
|---|---|---|
| all 8 (per-axis log std ×6, log std of acc magnitude, log mean of gyro magnitude) | 0.814 | 0.000 (`movement_mixed`) |
| log std of acc magnitude + log mean of gyro magnitude | 0.852 | 0.189 |
| **log std of acc magnitude only** | **1.000** | **1.000** |
| per-axis acc log std + log std of acc magnitude | 0.833 | 0.000 |

Tilting the board during hand movement gives as much rotation as shaking, and per-axis features depend on orientation; the acceleration-magnitude spread does not (0.28–0.75 m/s² for movement vs. 2.50–3.03 m/s² for shaking). Caveat: the feature was chosen on the same 6 sessions, so it still needs confirming on new recordings.

## 4. Training and evaluation (Python)
- Split by **whole recording** (session), not by window.
- Baseline: logistic regression or a small decision tree on the features (scikit-learn).
- Then a small MLP on the same features. The task notes that this beat a CNN on the raw signal.
- Use class weighting if the classes are imbalanced. Report the confusion matrix and per-class accuracy on the held-out sessions.

## 5. Deployment (board)
- `train.py` exports the scaler and logistic-regression weights as plain arrays to `firmware/include/model.h`.
- On-board loop: sample → ring buffer → features → model → class probabilities → smooth (average probabilities of the last 3 decisions) → LED colour.
- `check_board.py` rebuilds windows from the streamed raw samples and compares them with the board: features within 2e-3 (raw samples are printed with 3 decimals), probabilities within 1.5e-6.
- The same firmware streams raw samples over serial, so `record.py` works with it; the LED shows the prediction meanwhile.

## 6. Web page from the board (part 4)
- ESP32 Wi-Fi access point and an HTTP server.
- The page polls a JSON endpoint (or uses a WebSocket) for the latest samples and the class probabilities, and draws a live plot and probability bars.

## 7. BLE monitor (part 3)
- Firmware: a BLE GATT service with a state characteristic (UTF-8 text, notify).
- Check it with nRF Connect first.
- A separate **public** repository holds a GitHub Pages site (HTML/JS, Web Bluetooth). It has a Connect button and shows the current state, a timeline, alarms for long fast breathing or long "not attached" (limits chosen by us), and an alarm when the connection is lost.

## 8. Own extension (part 5)
**(open)**: postponed, to be decided after parts 1–4 work.

## 9. Documentation (part 6)
- README: group members, a link to the monitoring page, a wiring photo, build/flash steps, how data was recorded and the model trained, results and AI-tool usage.

## Commit plan (at least 3 pushed commits)
1. Docs + sensor read and LED test
2. Recording + training for part 1, board classifier for part 1
3. Four-class classifier
4. Board web page, BLE + monitoring page
5. Extension + README

## Open questions
Decided: Arduino + PlatformIO, 50 Hz.
1. ~~Model export~~: decided, hand-exported weights in `model.h`.
2. Group members and GitHub repository names.
3. Extension: postponed.
