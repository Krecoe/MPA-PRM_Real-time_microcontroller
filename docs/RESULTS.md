# Results

Only numbers we actually measured. Every cell that has not been measured says `TODO(fill)`.

## Dataset

18 sessions of 60 s each (3000 samples at 50 Hz), 954 windows of 8 s with a 1 s step.
Details of every session in [DATA_AND_TRAINING.md](DATA_AND_TRAINING.md).

| Class | Sessions | Minutes | Windows | People |
|-------|----------|---------|---------|--------|
| calm | 3 | 3 | 159 | TODO(fill) |
| fast | 3 | 3 | 159 | TODO(fill) |
| not_attached (movement + still) | 5 + 2 | 7 | 371 | TODO(fill) |
| shaking | 5 | 5 | 265 | TODO(fill) |

**Split:** leave-one-session-out. Each session is held out in turn and predicted by a model trained on
the other 17; the numbers below collect the predictions of all 18 held-out sessions.

## Model comparison

All on the same 6 features, leave-one-session-out accuracy over all 954 windows:

| Model | Parameters | Accuracy | Notes |
|-------|------------|----------|-------|
| logistic regression (baseline) | 28 | 0.775 | balanced class weights |
| random forest, 200 trees | - | 0.868 | balanced class weights |
| MLP, 16 hidden units | 180 | 0.878 | no class weighting |
| **MLP, 16 hidden units (shipped)** | 180 | **0.884** | balanced sample weights |

Feature ablation (accuracy):

| Model | all 6 | without gyro | without high-freq share | amplitudes only |
|-------|-------|--------------|-------------------------|-----------------|
| logistic regression | 0.775 | 0.771 | 0.776 | 0.510 |
| MLP, 16 hidden units | 0.878 | 0.877 | 0.814 | 0.682 |
| random forest | 0.868 | 0.853 | 0.794 | 0.687 |

Part 1 only (movement vs. shaking, the first 6 sessions, logistic regression): with the per-axis and
rotation features 0.814 (the tilted-movement session was 0 % correct); with only the log std of the
acceleration magnitude 1.000. That feature was chosen on the same 6 sessions, so the 1.000 is
optimistic.

Per class, shipped model:

| Class | Precision | Recall | Windows |
|-------|-----------|--------|---------|
| calm | 0.864 | 0.881 | 159 |
| fast | 0.782 | 0.855 | 159 |
| not_attached | 0.896 | 0.887 | 371 |
| shaking | 0.948 | 0.898 | 265 |

Per held-out session (fraction of windows correct):

| Session | Accuracy | Session | Accuracy |
|---------|----------|---------|----------|
| calm sitting | 1.000 | movement vertical | 1.000 |
| calm sitting-2 | 1.000 | movement smooth | 0.509 |
| calm standing | 0.642 | movement fast-walking | 0.962 |
| fast sitting | 0.868 | shaking flat | 1.000 |
| fast sitting-2 | 1.000 | shaking vertical | 1.000 |
| fast standing | 0.698 | shaking mixed | 0.925 |
| movement flat | 0.868 | shaking gentle | 0.566 |
| movement mixed | 0.868 | shaking gentle-vertical | 1.000 |
| still table-flat | 1.000 | still table-side | 1.000 |

## Confusion matrix (held-out sessions)

| predicted →<br>actual ↓ | calm | fast | not_attached | shaking |
|--------------------------|------|------|--------------|---------|
| calm | 140 | 16 | 3 | 0 |
| fast | 12 | 136 | 11 | 0 |
| not_attached | 10 | 19 | 329 | 13 |
| shaking | 0 | 3 | 24 | 238 |

The main confusions are calm vs. fast (28 windows) and not_attached vs. the other classes. The worst
held-out sessions (`movement_smooth`, `shaking_gentle`, `calm_standing`, `fast_standing`) are each the
only session of their kind, so nothing similar is in the training data when they are held out. The
model on the board is trained on all 18 sessions. More sessions of these kinds, ideally from other
people, are the most direct fix.

## On the board

| Measurement | Value |
|-------------|-------|
| Features + network per decision | 43.2 ms (mean of the `# T` timings over one `check_board.py` run, `micros()` around `computeFeatures()` + `predict()`) |
| Window length / step | 8 s / 1 s; two features use the last 2 s |
| Smoothing | mean of the probabilities of the last 3 decisions (3 s) |
| Switching movement ↔ shaking on the LED | about 5 s (observed live, not measured precisely) |
| Flash usage | 1 377 799 B of 3 342 336 B (41.2 %), with BLE, Wi-Fi and the web server |
| Static RAM usage | 76 368 B of 327 680 B (23.3 %) |
| Prediction matches Python on the same window | yes: features within 0.004, probabilities within 0.006 (`check_board.py`) |
| BLE + Wi-Fi hotspot + classifier together | all three worked at the same time in our tests; long-term stability not measured |

## Live demonstration

What we showed the teacher, and what happened: TODO(fill).

Before the demo, all four states worked in our own live test (movement → blue, shaking → magenta,
calm breathing on the chest → green, fast breathing → red, on the table → blue). The BLE monitor
connected from a Windows PC in Chrome and showed the state; the board's web page worked over its
hotspot.

## What we would do next

- Record more sessions of the weakest kinds (smooth movement, gentle shaking, breathing while
  standing), ideally from other people, and re-evaluate.
- Shorten the time to switch between movement and shaking (about 5 s), e.g. by deciding shaking from
  the 2 s features alone.
