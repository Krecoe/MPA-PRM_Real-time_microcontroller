# Use of AI tools

The assignment allows AI tools but requires us to say which ones and for what, and every group member
must be able to explain all committed code.

## Tools we used

| Tool | Who | Used for |
|------|-----|----------|
| Claude Code (Anthropic, CLI agent) | Sebastian | Read the assignment page and the existing repo docs, planned the pipeline, wrote the Web Bluetooth monitoring page in `monitor/`, wrote `docs/BLE_PROTOCOL.md` (GATT contract and the example firmware side), wrote this documentation template, and tested the monitor page in headless Chrome |
| TODO(fill) | TODO(fill) | TODO(fill) — e.g. anything Kristof used for the firmware or training code |

## What the AI tool did **not** do

Worth stating plainly, because it is the part that matters for grading:

- it did not record any sensor data, and has not seen our signal
- it did not train the model and did not measure any accuracy — every number in
  [RESULTS.md](RESULTS.md) that is not `TODO(fill)` was measured by a human
- it did not write the firmware on `main`; it proposed a BLE interface that has to be checked against
  what actually runs on the board
- its work was tested by loading the page in a browser and reading the console, not assumed to work

## How it was used, in practice

- Reading the assignment text and turning it into the graded checklist in `GOALS.md`.
- Drafting code we then had to make run: the first version of the monitor page threw a JavaScript
  error that silently disabled every alarm and the timeline; only running it in a browser caught it.
  Treat AI-written code the same way — run it, read the output.
- Anything copied from an AI tool still has to be explainable line by line when the teacher asks.

## Statement

Every member of the group can explain the committed code, including the parts drafted with AI help.
TODO(fill): confirm before handing in.
