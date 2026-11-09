"""Record a labelled session of sensor data streamed by the board over USB serial.

Usage: python record.py <label> [seconds] [variant]
Saves data/<label>_<variant>_<YYYYmmdd_HHMMSS>.csv with columns t_ms,ax,ay,az,gx,gy,gz.
The class label is always the first part of the file name; the variant (e.g. flat,
vertical, mixed) only documents how the session differs.
"""
import sys
import time
from datetime import datetime
from pathlib import Path

import serial

PORT = "COM7"
BAUD = 115200
HEADER = "t_ms,ax,ay,az,gx,gy,gz"

label = sys.argv[1]
duration_s = float(sys.argv[2]) if len(sys.argv) > 2 else 60
variant = sys.argv[3] if len(sys.argv) > 3 else "default"
assert "_" not in label and "_" not in variant, "use '-' instead of '_' in label and variant"

Path("data").mkdir(exist_ok=True)
out_path = Path("data") / f"{label}_{variant}_{datetime.now():%Y%m%d_%H%M%S}.csv"

with serial.Serial(PORT, BAUD, timeout=1) as port:
    port.reset_input_buffer()
    port.readline()  # drop a possibly partial first line
    rows = []
    print(f"Recording '{label}' ({variant}) for {duration_s:.0f} s ...")
    end_time = time.time() + duration_s
    while time.time() < end_time:
        line = port.readline().decode(errors="replace").strip()
        if line.count(",") == 6 and line[0].isdigit():
            rows.append(line)

out_path.write_text(HEADER + "\n" + "\n".join(rows) + "\n")
first_ms, last_ms = int(rows[0].split(",")[0]), int(rows[-1].split(",")[0])
rate_hz = 1000 * (len(rows) - 1) / (last_ms - first_ms)
print(f"Saved {len(rows)} samples ({rate_hz:.2f} Hz) to {out_path}")
