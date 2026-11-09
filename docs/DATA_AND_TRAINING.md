# Data recording and training

How the dataset was made and how the model was fitted, so that someone else can repeat it.
Anything not measured or not done yet is marked `TODO(fill)`.

## Classes

| Label | Meaning | LED |
|-------|---------|-----|
| `calm` | sensor pressed to the chest, calm breathing | green |
| `fast` | sensor on the chest, deliberately fast breathing | red |
| `not_attached` | held in the hand, or the board lying still on the table | blue |
| `shaking` | shaken (the whole breadboard, not just the sensor) | magenta |

Graded part 1 uses only `not_attached` (slow movement in the hand) versus `shaking`. The other two
classes come in for part 2.

## Recording

- The recording firmware streams samples over USB serial. This is the only stage where serial data is
  allowed; in the final demonstration USB carries power only.
- A Python script writes one CSV per session:

```
data/<label>_<session>.csv
t_ms,ax,ay,az,gx,gy,gz
0,0.02,-0.11,9.79,0.01,0.00,0.02
```

  `t_ms` = milliseconds since boot, accel in m/s², gyro in rad/s, 50 Hz.
- CSVs are **never committed** (see CLAUDE.md). The training script reads the folder from disk.
- Command used: TODO(fill): exact command line, e.g. `python data/record.py calm 3`
- Sampling settings at record time: 50 Hz, ±4 g, ±500 °/s, 21 Hz low-pass — identical to the
  firmware that runs the classifier, because features must match on both sides.

### What we recorded

| Class | Sessions | Total duration | People | Notes |
|-------|----------|----------------|--------|-------|
| calm | TODO(fill) | TODO(fill) | TODO(fill) | TODO(fill): position, clothing |
| fast | TODO(fill) | TODO(fill) | TODO(fill) | TODO(fill): how fast, metronome or not |
| not_attached | TODO(fill) | TODO(fill) | TODO(fill) | include the board lying still |
| shaking | TODO(fill) | TODO(fill) | TODO(fill) | TODO(fill) |

Recording conditions and gotchas: TODO(fill) — e.g. room noise, who wore what, sessions that had to
be thrown away and why. Sessions from several people and several positions matter: a model trained
on one chest usually fails on another.

## Features

Sliding windows over each recording, computed identically in Python (training) and in C++ (board):

- window length / step: TODO(fill) — breathing is 0.1–0.5 Hz, so windows must span several breaths
- per axis and per magnitude: mean, standard deviation, range, energy, dominant frequency or band
  power — TODO(fill): which ones survived
- positive amplitude features are log-transformed (a tip from the task that also helped us:
  TODO(fill) — state whether it actually helped, or drop this line)

Keep this list honest: if the final feature vector differs from the plan, describe what was
**actually** used, because the board has to reproduce it exactly.

## Training and evaluation

- **Split by whole recording session**, before cutting into windows. Windows from one session leak
  into each other, and a random window split produces an accuracy number that means nothing.
  Sessions in the test set: TODO(fill)
- Baseline first: TODO(fill) — e.g. logistic regression on the features above
- Then: TODO(fill) — e.g. a small MLP on the same features
- Class weighting if the classes are unbalanced: TODO(fill) — used or not, and the class counts
- Anything dropped along the way (CNN on the raw signal etc.) and why: TODO(fill)

## Export to the board

TODO(fill): how the fitted model becomes C code — hand-written weights header, `everywhereml`,
`micromlgen` or TFLite Micro — and the file it produces.

The check that matters: feed the same window to Python and to the board and compare the outputs.
Result of that check: TODO(fill). If they disagree, the feature code differs (units, filter, window
alignment) — that is the first place to look when the board disagrees with the validation score.

## Reproduce

```bash
python -m venv .venv && source .venv/Scripts/activate   # Windows
pip install -r training/requirements.txt                # TODO(fill): create this file
python training/train.py                                # prints metrics, writes the model header
```
