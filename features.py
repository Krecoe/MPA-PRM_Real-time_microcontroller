"""Window settings and features shared by train.py and check_board.py.
window_features() must match computeFeatures() in firmware/src/main.cpp."""
import numpy as np

SAMPLE_RATE_HZ = 50
WINDOW_S = 8  # long enough to contain at least two breaths later on
STEP_S = 1
WINDOW = WINDOW_S * SAMPLE_RATE_HZ
STEP = STEP_S * SAMPLE_RATE_HZ
EPS = 1e-6

# Only orientation-independent amplitude: the per-axis and rotation features made the model
# call tilting hand movement "shaking" (see the feature comparison in PIPELINE.md)
FEATURE_NAMES = ["log_std_acc_mag"]


def window_features(window):
    """window: (WINDOW, 6) array of ax, ay, az, gx, gy, gz."""
    acc_mag = np.linalg.norm(window[:, :3], axis=1)
    # Amplitudes span orders of magnitude between breathing and shaking, so use their logs
    return np.array([np.log(acc_mag.std() + EPS)])
