# Use of AI tools

The assignment allows AI tools but requires us to say which ones and for what, and every group member
must be able to explain all committed code.

## Tools we used

| Tool | Who | Used for |
|------|-----|----------|
| Claude Code (Anthropic, CLI agent) | Sebastian | Read the assignment page and the existing repo docs, planned the pipeline, wrote the Web Bluetooth monitoring page in `monitor/`, wrote `docs/BLE_PROTOCOL.md` (GATT contract and the example firmware side), wrote this documentation template, and tested the monitor page in headless Chrome |
| Claude Code (Anthropic, CLI agent, model Claude Opus 5.5) | Kristof | Read the assignment and wrote `GOALS.md` / `PIPELINE.md`; wrote all firmware on `main` (sensor test, features, MLP inference, smoothing, LED, BLE, Wi-Fi hotspot and web page); wrote `record.py`, `features.py`, `train.py`, `check_board.py`; built and flashed the board and read its serial output; ran the training and the feature/model comparisons on our recorded data; merged `Seba_branch` and adapted the firmware to its BLE contract; filled in this documentation from the measured outputs |

## What the humans did

- Wired the board and the sensor.
- Recorded every session: held, moved and shook the board, wore it on the chest and breathed.
- Made the decisions the tool asked about: toolchain (Arduino + PlatformIO), 50 Hz sampling, adding
  more data for the hard cases, an open Wi-Fi hotspot, using Seba's monitoring page.
- Tested everything that can only be seen live: LED colours, how fast the states switch, the BLE
  monitor in Chrome on Windows, and the board's web page over its hotspot.

## What the AI tools did **not** do

- They did not record or modify any sensor data.
- They did not invent numbers: every number in [RESULTS.md](RESULTS.md) comes from running
  `train.py`, the comparison scripts, `check_board.py` or the PlatformIO build on our data and board.
  Things that were observed but not measured (e.g. about 5 s to switch on the LED) are marked as such.
- The monitoring page (Sebastian) was written before the firmware; it was checked against the real
  board only when Kristof connected to it from Chrome.

## How it was used, in practice

- Reading the assignment text and turning it into the graded checklist in `GOALS.md`.
- Every code change was built and run on the board before it was called working, and the board's
  output was compared with Python (`check_board.py`).
- Problems found that way, not by reading the code: a constant named `EPS` and an array named `B1`
  clashed with macros in the ESP32 and Arduino headers; the first part-1 model failed completely on
  one session (tilted hand movement), which led to orientation-independent features.
- Sebastian's first version of the monitor page threw a JavaScript error that silently disabled every
  alarm and the timeline; only running it in a browser caught it.
- Anything written with an AI tool still has to be explainable line by line when the teacher asks.

## Statement

Every member of the group can explain the committed code, including the parts drafted with AI help.
TODO(fill): confirm before handing in.
