# Real-time AI on a microcontroller - breathing and motion states

MPA-PRM (Computer-Aided Medical Diagnostics), Application IV. A 4-hour graded group project:
an ESP32-S3 with an accelerometer and gyroscope that classifies what is happening to it, entirely on
the chip, and shows the result on an RGB LED, on a web page it serves itself, and on a bedside-style
monitor over Bluetooth Low Energy.

**Group members:** Kristof Krupa (GitHub: kristofkrupa), Sebastian Černý (GitHub: Krecoe)
<!-- TODO(fill): confirm the split of work below -->
- Kristof Krupa: wiring, data recording, features and training, firmware (classifier, LED, BLE, board web page)
- Sebastian Černý: BLE monitoring page and BLE contract, GitHub Pages, documentation templates

**Monitoring page:** <https://krecoe.github.io/MPA-PRM_Real-time_microcontroller/monitor/>.
Open it in Chrome or Edge on Windows, macOS or Android (not iPhone, not Firefox); see
[monitor/README.md](monitor/README.md).

## What it does

| LED colour | State | Meaning |
|------------|-------|---------|
| green | `calm` | calm breathing, sensor on the chest |
| red | `fast` | fast breathing, sensor on the chest |
| blue | `not_attached` | held in the hand or lying still |
| magenta | `shaking` | being shaken |

Everything after the flashing step runs on the ESP32-S3: sampling, feature extraction,
classification, smoothing, the LED, the Wi-Fi web page and the BLE notifications. The USB cable
supplies power only during a demonstration; the computer is used for recording, training and flashing.

The same state is shown in three places:
- the on-board **RGB LED**;
- the **board's own web page**: join the open Wi-Fi hotspot `ESP32-BREATH` and open
  <http://192.168.4.1/>: live x/y/z acceleration of the last 8 s, the state, and the probabilities
  of all four classes;
- the **BLE monitoring page** above: current state, timeline, alarms for long fast breathing, long
  "not attached" and a lost connection.

## Photo of the wiring

![Wiring of the ESP32-S3 and the MPU-6050 module](docs/wiring.jpg)

> **TODO(fill): take this photo DURING the lab** and save it as `docs/wiring.jpg`. It is a graded
> item and it cannot be taken after the boards have been returned.

## Wiring

ESP32-S3 N16R8 devkit and an MPU-6050 module (our board is labelled HW-123, equivalent to GY-521).
The module has the I2C pull-ups on board, so no external resistors are needed.

| MPU-6050 / HW-123 | ESP32-S3 |
|-------------------|----------|
| VCC | 3.3 V |
| GND | GND |
| SDA | GPIO10 |
| SCL | GPIO11 |

The I2C address is `0x68` (AD0 low, the default on these modules); our I2C scan found exactly that.
The status LED is the addressable WS2812 on **GPIO48**, driven with `rgbLedWrite()` - no external LED.
The board is connected to the computer by USB-C (on our devkit it appears as a CH343 serial port, COM7).

In use, hold the whole breadboard rather than the small sensor board, and press it against the chest
for the breathing classes.

## Quick rebuild

From the repository root (details in [docs/BUILD_AND_FLASH.md](docs/BUILD_AND_FLASH.md) and
[docs/DATA_AND_TRAINING.md](docs/DATA_AND_TRAINING.md)):

```bash
python -m venv .venv
.venv/Scripts/python -m pip install -r requirements.txt     # Windows; bin/ instead of Scripts/ elsewhere
cd firmware && ../.venv/Scripts/pio run -t upload && cd ..   # flash the firmware (it also streams raw data)
.venv/Scripts/python record.py calm 60 sitting               # record labelled 60 s sessions into data/
.venv/Scripts/python train.py                                # evaluate, then write firmware/include/model.h
cd firmware && ../.venv/Scripts/pio run -t upload && cd ..   # flash again with the new model
.venv/Scripts/python check_board.py 20                       # optional: board vs. Python on the same windows
```

After the last flash the board works on its own: power it from any USB supply.

## How it works

Sensor → 50 Hz sampling → 8 s sliding window → 6 hand-made features → small neural network →
smoothing → LED / web page / BLE, all on the board.

- **Sampling:** 50 Hz, ±4 g accelerometer (so shaking does not clip), ±500 °/s gyroscope, 21 Hz
  low-pass filter - below the 25 Hz Nyquist limit.
- **Window:** 8 s (400 samples), a new decision every 1 s. Calm breathing is slow, so the window has
  to contain at least two breaths. Two features use only the last 2 s, so movement vs. shaking reacts
  faster.
- **Features** (all independent of how the board is oriented; [features.py](features.py) and
  `computeFeatures()` in [firmware/src/main.cpp](firmware/src/main.cpp)):
  1. log of the total acceleration amplitude over 8 s (square root of the covariance trace)
  2. log of the standard deviation of the acceleration magnitude over the last 2 s
  3. share of the power of the acceleration magnitude above 3 Hz over the last 2 s
  4. share of power in 0.125-1.5 Hz along the principal direction of acceleration change
     (breathing tilts the chest along one direction)
  5. strongest frequency in that band, in breaths per minute
  6. log of the mean rotation speed (gyroscope magnitude)
- **Model:** standardization + MLP with one hidden layer of 16 ReLU units and a softmax output
  (180 parameters), trained with scikit-learn and exported as plain C arrays in
  [firmware/include/model.h](firmware/include/model.h).
- **Smoothing:** the class probabilities of the last 3 decisions (3 s) are averaged; the LED shows
  the class with the highest average.

## Results

Only numbers we measured; details, confusion matrix and the split in [docs/RESULTS.md](docs/RESULTS.md).
Evaluation is leave-one-session-out over 18 recorded sessions (954 windows): every window of the
tested session is excluded from training.

| Model (6 features) | Parameters | Accuracy | Notes |
|--------------------|------------|----------|-------|
| logistic regression (baseline) | 28 | 0.775 | balanced class weights |
| random forest, 200 trees | - | 0.868 | balanced class weights |
| **MLP, 16 hidden units (shipped)** | 180 | **0.884** | balanced sample weights |

On the board: features + network take **43 ms** per decision (measured with `micros()`), the
firmware uses 41.2 % of the app flash and 23.3 % of the static RAM with BLE, Wi-Fi and the web server.
Board and Python agree on the same windows (probabilities within 0.006).

## Repository map

| Path | What |
|------|------|
| `firmware/` | PlatformIO project (Arduino framework): sensor, features, classifier, LED, BLE, Wi-Fi web page |
| `firmware/include/model.h` | trained model, generated by `train.py` |
| `firmware/include/web_page.h` | the page the board serves at http://192.168.4.1/ |
| `record.py` | records a labelled session from the board over USB serial |
| `features.py` | window settings and features (mirrored in the firmware) |
| `train.py` | leave-one-session-out evaluation, final training, export to `model.h` |
| `check_board.py` | compares the board's features and probabilities with Python |
| `monitor/` | Web Bluetooth monitoring page, published on GitHub Pages |
| `index.html` | GitHub Pages landing page |
| `data/` | recorded CSVs - **never committed** |
| `docs/` | build instructions, data and training, results, BLE contract, AI-tool usage |
| `GOALS.md`, `PIPELINE.md` | task breakdown and how we planned and decided things during the lab |

## AI tools

We used AI tools; which ones and for what is listed in [docs/AI_TOOLS.md](docs/AI_TOOLS.md), as the
assignment requires. Every group member can explain all committed code.

## Acknowledgements

Assignment text: https://tomasvicar.github.io/AUI-public/labs/real-time-ai.html
