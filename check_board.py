"""Check that the board computes the same features and probabilities as Python.

Captures the classifier firmware's serial output, rebuilds each window from the streamed raw samples,
and recomputes the features and probabilities with features.py and the exported model.h.
Usage: python check_board.py [seconds]
"""
import re
import sys
import time
from pathlib import Path

import numpy as np
import serial

from features import WINDOW, window_features

PORT = "COM7"
duration_s = float(sys.argv[1]) if len(sys.argv) > 1 else 20


def read_array(header_text, name):
    body = re.search(name + r"\[[^=]*=\s*\{([^;]*)\};", header_text).group(1)
    return np.array([float(v.rstrip("f")) for v in body.split(",")])


header_text = Path("firmware/include/model.h").read_text()
feature_mean = read_array(header_text, "FEATURE_MEAN")
feature_scale = read_array(header_text, "FEATURE_SCALE")
bias = read_array(header_text, "BIAS")
weights = read_array(header_text, "WEIGHTS").reshape(len(bias), -1)

raw_samples = []
comparisons = []
with serial.Serial(PORT, 115200, timeout=1) as port:
    port.reset_input_buffer()
    port.readline()
    end_time = time.time() + duration_s
    while time.time() < end_time:
        line = port.readline().decode(errors="replace").strip()
        if line.count(",") == 6 and line[0].isdigit():
            raw_samples.append([float(v) for v in line.split(",")[1:]])
        elif line.startswith("# P,") and len(raw_samples) >= WINDOW:
            parts = line.split(",")
            n_features = len(feature_mean)
            board_features = np.array(parts[2:2 + n_features], dtype=float)
            board_probabilities = np.array(parts[2 + n_features:-1], dtype=float)
            python_features = window_features(np.array(raw_samples[-WINDOW:]))
            logits = weights @ ((python_features - feature_mean) / feature_scale) + bias
            python_probabilities = np.exp(logits - logits.max()) / np.exp(logits - logits.max()).sum()
            comparisons.append((board_features, python_features, board_probabilities, python_probabilities, parts[-1]))

print(f"Compared {len(comparisons)} windows")
for board_f, python_f, board_p, python_p, state in comparisons:
    print(f"  features board {np.round(board_f, 4)} python {np.round(python_f, 4)} | "
          f"probabilities board {np.round(board_p, 4)} python {np.round(python_p, 4)} | state {state}")
max_feature_diff = max(np.abs(b - p).max() for b, p, _, _, _ in comparisons)
max_probability_diff = max(np.abs(b - p).max() for _, _, b, p, _ in comparisons)
print(f"Max feature difference: {max_feature_diff:.2e}, max probability difference: {max_probability_diff:.2e}")
