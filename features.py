"""Window settings and features shared by train.py and check_board.py.
window_features() must match computeFeatures() in firmware/src/main.cpp, so every step here
is written the way the board does it (direct DFT of a few bins, power iteration, no library tricks).
"""
import numpy as np

SAMPLE_RATE_HZ = 50
WINDOW_S = 8  # long enough to contain at least two calm breaths
STEP_S = 1
WINDOW = WINDOW_S * SAMPLE_RATE_HZ
STEP = STEP_S * SAMPLE_RATE_HZ
SHORT = 2 * SAMPLE_RATE_HZ  # the last 2 s: movement vs. shaking must react faster than breathing
EPS = 1e-6

# Bin k of an N-point DFT is at k * SAMPLE_RATE_HZ / N Hz
BREATH_BINS = range(1, 13)  # 0.125-1.5 Hz = 7.5-90 breaths per minute for the 400-sample window
LOW_BINS_SHORT = range(0, 7)  # 0-3 Hz for the 100-sample short window

FEATURE_NAMES = ["log_acc_total_std", "log_acc_mag_std_2s", "high_freq_share_2s",
                 "breath_band_share", "breath_rate_bpm", "log_gyro_mean"]


def hann(n):
    return 0.5 - 0.5 * np.cos(2 * np.pi * np.arange(n) / n)


def bin_power(signal, k):
    """|X_k|^2 of the DFT, computed directly as the board does."""
    phase = 2 * np.pi * k * np.arange(len(signal)) / len(signal)
    return np.sum(signal * np.cos(phase)) ** 2 + np.sum(signal * np.sin(phase)) ** 2


def principal_direction(deviations):
    """Direction of largest acceleration change, by power iteration on the 3x3 covariance.
    Breathing tilts the chest along one direction, whatever way the board is mounted."""
    covariance = deviations.T @ deviations / len(deviations)
    direction = np.ones(3) / np.sqrt(3)
    for _ in range(30):
        direction = covariance @ direction
        direction /= np.linalg.norm(direction) + EPS
    return direction


def window_features(window):
    """window: (WINDOW, 6) array of ax, ay, az, gx, gy, gz."""
    acc = window[:, :3]
    gyro = window[:, 3:]
    deviations = acc - acc.mean(axis=0)
    # Square root of the covariance trace = total amplitude, independent of orientation
    acc_total_std = np.sqrt(np.mean(np.sum(deviations ** 2, axis=1)))

    # Last 2 s of the acceleration magnitude: amplitude and share of power above 3 Hz
    short_mag = np.linalg.norm(acc[-SHORT:], axis=1)
    short_signal = (short_mag - short_mag.mean()) * hann(SHORT)
    short_energy = np.sum(short_signal ** 2) + EPS
    # Parseval: total energy = (|X_0|^2 + 2 * sum of the other one-sided bins) / N
    low_energy = (bin_power(short_signal, 0) + 2 * sum(bin_power(short_signal, k) for k in LOW_BINS_SHORT if k > 0)) / SHORT
    high_freq_share = 1 - low_energy / short_energy

    # Breathing: spectrum of the acceleration along its principal direction
    breath_signal = (deviations @ principal_direction(deviations)) * hann(WINDOW)
    breath_energy = np.sum(breath_signal ** 2) + EPS
    breath_powers = np.array([bin_power(breath_signal, k) for k in BREATH_BINS])
    breath_band_share = 2 * breath_powers.sum() / WINDOW / breath_energy
    breath_rate_bpm = BREATH_BINS[int(np.argmax(breath_powers))] * SAMPLE_RATE_HZ / WINDOW * 60

    gyro_mean = np.mean(np.linalg.norm(gyro, axis=1))
    # Amplitudes span orders of magnitude between lying still and shaking, so use their logs
    return np.array([np.log(acc_total_std + EPS), np.log(short_mag.std() + EPS), high_freq_share,
                     breath_band_share, breath_rate_bpm, np.log(gyro_mean + EPS)])
