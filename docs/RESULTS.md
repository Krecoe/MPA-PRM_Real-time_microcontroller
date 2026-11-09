# Results

Only numbers we actually measured. Every cell that has not been measured says `TODO(fill)`; nothing
here is an estimate, an expectation or a figure from the assignment text.

## Dataset

| Class | Sessions | Total minutes | People |
|-------|----------|---------------|--------|
| calm | TODO(fill) | TODO(fill) | TODO(fill) |
| fast | TODO(fill) | TODO(fill) | TODO(fill) |
| not_attached | TODO(fill) | TODO(fill) | TODO(fill) |
| shaking | TODO(fill) | TODO(fill) | TODO(fill) |

Train / test split by whole recording session (never by window):

- training sessions: TODO(fill)
- held-out test sessions: TODO(fill)

## Model comparison

Accuracy on the held-out sessions:

| Model | Parameters | Val accuracy | Notes |
|-------|------------|--------------|-------|
| TODO(fill): baseline, e.g. logistic regression | TODO(fill) | TODO(fill) | TODO(fill) |
| TODO(fill): e.g. small MLP | TODO(fill) | TODO(fill) | TODO(fill) |

Per-class accuracy of the model we shipped:

| Class | Accuracy | Support (windows) |
|-------|----------|-------------------|
| calm | TODO(fill) | TODO(fill) |
| fast | TODO(fill) | TODO(fill) |
| not_attached | TODO(fill) | TODO(fill) |
| shaking | TODO(fill) | TODO(fill) |

## Confusion matrix (held-out sessions)

| predicted →<br>actual ↓ | calm | fast | not_attached | shaking |
|--------------------------|------|------|--------------|---------|
| calm | TODO(fill) | TODO(fill) | TODO(fill) | TODO(fill) |
| fast | TODO(fill) | TODO(fill) | TODO(fill) | TODO(fill) |
| not_attached | TODO(fill) | TODO(fill) | TODO(fill) | TODO(fill) |
| shaking | TODO(fill) | TODO(fill) | TODO(fill) | TODO(fill) |

Which confusions matter most, and what we did about them: TODO(fill) — the one worth commenting on
is usually calm vs fast, since both are breathing and differ mainly in rate and amplitude.

## On the board

| Measurement | Value |
|-------------|-------|
| Inference time per window | TODO(fill) ms |
| Window length / step | TODO(fill) |
| Time from sample to LED | TODO(fill) — including smoothing |
| Flash usage | TODO(fill) |
| RAM usage | TODO(fill) |
| Prediction matches Python on the same window | TODO(fill): yes/no, how checked |
| BLE + Wi-Fi AP + classifier together | TODO(fill): stable? any restarts? |

How it was measured (e.g. `micros()` around the inference call, averaged over N windows): TODO(fill)

## Live demonstration

What we showed the teacher, and what happened: TODO(fill) — including any class that misbehaved in
front of the teacher. Honest reporting of a flaky class is worth more than a claim that was not
tested.

Known weaknesses: TODO(fill) — e.g. fast breathing from a second person, loose sensor, cable dragging
the breadboard.

## What we would do next

TODO(fill) — only things we actually observed as problems.
