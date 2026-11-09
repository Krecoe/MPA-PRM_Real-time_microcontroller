# Data recording and training

How the dataset was made and how the model was fitted, so that someone else can repeat it.
Anything not known yet is marked `TODO(fill)`.

## Classes

| Device state | Recording labels | Meaning | LED |
|--------------|------------------|---------|-----|
| `calm` | `calm` | sensor pressed to the chest, calm breathing | green |
| `fast` | `fast` | sensor on the chest, deliberately fast breathing | red |
| `not_attached` | `movement`, `still` | held and moved in the hand, or lying still on the table | blue |
| `shaking` | `shaking` | shaken (the whole breadboard, not just the sensor) | magenta |

`movement` and `still` are recorded separately and merged into `not_attached` in `train.py`
(`CLASS_OF_LABEL`). Graded part 1 used only `movement` vs. `shaking`.

## Recording

- The firmware always streams raw samples over USB serial (`t_ms,ax,ay,az,gx,gy,gz`, accel in m/s²,
  gyro in rad/s, 50 Hz), so recording needs no special firmware. In the final demonstration nothing
  reads the serial port; the USB cable only powers the board.
- `record.py` saves one CSV per session:

```bash
.venv/Scripts/python record.py <label> [seconds] [variant]
.venv/Scripts/python record.py calm 60 sitting
# -> data/calm_sitting_20261109_141757.csv
```

  The class label is the first part of the file name; the variant only documents how the session
  differs (use `-`, not `_`, inside label and variant). The script prints the number of samples and
  the measured rate; all our sessions have 3000 samples at 50.00 Hz.
- CSVs are **never committed** (`data/` is in `.gitignore`).
- Sampling settings: 50 Hz, ±4 g, ±500 °/s, 21 Hz low-pass - the same firmware records and classifies.

### What we recorded

All sessions are 60 s, recorded on 2026-11-09.

| Label | Sessions (variants) | Total | Notes |
|-------|---------------------|-------|-------|
| calm | 3 (sitting, sitting-2, standing) | 3 min | TODO(fill): pace, clothing |
| fast | 3 (sitting, sitting-2, standing) | 3 min | TODO(fill): pace, metronome or not |
| movement | 5 (flat, vertical, mixed, smooth, fast-walking) | 5 min | held flat, vertical, tilted and turned, slowly, while walking |
| still | 2 (table-flat, table-side) | 2 min | lying on the table in two orientations |
| shaking | 5 (flat, vertical, mixed, gentle, gentle-vertical) | 5 min | vigorous and gentle, in different directions |

People who recorded: TODO(fill) - how many people, and who wore the sensor for the breathing sessions.

The first 6 sessions (movement and shaking, flat/vertical/mixed) were recorded for part 1. After the
first live test, the LED switched too slowly and confused gentle shaking with movement and fast
movement with shaking, so we added the hard cases (`gentle`, `gentle-vertical`, `smooth`,
`fast-walking`) together with the breathing and still sessions.

## Features

Sliding windows of 8 s (400 samples) with a step of 1 s, computed the same way in Python
([features.py](../features.py)) and on the board (`computeFeatures()` in
[firmware/src/main.cpp](../firmware/src/main.cpp)):

| Feature | Window | Why |
|---------|--------|-----|
| log of the total acceleration amplitude (square root of the covariance trace) | 8 s | overall motion level: still < breathing < movement < shaking |
| log of the std of the acceleration magnitude | last 2 s | shaking amplitude, reacting faster than 8 s |
| share of the power of the acceleration magnitude above 3 Hz | last 2 s | shaking is a fast back-and-forth (about 4-5 Hz); even fast hand movement is slower |
| share of power in 0.125-1.5 Hz along the principal direction of acceleration change | 8 s | breathing and slow movement are in this band, shaking is not |
| strongest frequency in that band, in breaths/min | 8 s | calm vs. fast: session medians 15-22.5 /min for calm, 37.5 /min for fast |
| log of the mean rotation speed | 8 s | breathing barely rotates the board, hand movement does |

All features are independent of how the board is oriented. That was the main lesson of part 1:
the first model used the standard deviation of each axis plus the rotation, and it classified a whole
session of tilted hand movement (`movement_mixed`) as shaking (0 % correct when that session was held
out). The amplitude features are log-transformed because they span orders of magnitude between lying
still and shaking (the task's tip).

The spectra use a Hann window and compute only the needed DFT bins directly (no FFT); the principal
direction is found by 30 steps of power iteration on the 3×3 covariance matrix. Both are written the
same way in Python and C++, so the board reproduces the features exactly.

## Training and evaluation

- **Leave one session out:** each of the 18 sessions is held out in turn, the model is trained on the
  windows of the other 17, and the held-out windows are predicted. Windows of one session overlap and
  are never split between training and testing.
- **Baseline first:** logistic regression on the 6 features (balanced class weights): 0.775.
- **Final model:** standardization + MLP with 16 ReLU hidden units (`sklearn.neural_network.MLPClassifier`,
  `random_state=0`), 0.884. A random forest (200 trees) reached 0.868.
- **Class balance:** `not_attached` has 371 windows, `shaking` 265, `calm` and `fast` 159 each, so
  training uses balanced sample weights (`compute_sample_weight("balanced")`).
- **Feature choice:** removing the high-frequency share lowered the MLP from 0.878 to 0.814;
  removing the gyroscope feature barely changed it (0.877). Full table in
  [PIPELINE.md](../PIPELINE.md) and [RESULTS.md](RESULTS.md).
- **Not tried:** a CNN on the raw signal (the task notes that a small network on hand-made features
  did better in the teacher's recordings).

## Export to the board

`train.py` trains the final model on all sessions and writes
[firmware/include/model.h](../firmware/include/model.h): feature means and scales of the
standardization, hidden-layer weights and biases, output weights and biases, and the class names, as
plain `const float` arrays. The firmware applies standardization → ReLU layer → softmax itself
(`predict()` in `main.cpp`); no ML library runs on the board.

**Check:** `check_board.py` reads the board's serial output, rebuilds each window from the streamed
raw samples, and recomputes features and probabilities in Python. Result: features within 0.004 and
probabilities within 0.006. The small difference comes from the raw samples being printed with
3 decimals.

## Reproduce

```bash
python -m venv .venv
.venv/Scripts/python -m pip install -r requirements.txt   # Windows; .venv/bin/python elsewhere
.venv/Scripts/python train.py                             # prints the results, writes firmware/include/model.h
```

`train.py` is deterministic (`random_state=0`); running it again on the same data gave the same
numbers and an identical `model.h`.
