# ==========================================
# 1. IMPORTS
# ==========================================
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
from scipy.signal import savgol_filter, correlate, detrend, find_peaks

# ==========================================
# 2. USER INPUTS
# ==========================================
file_upper = "ISAC3_top_123.TXT"
file_lower = "ISAC3_mid_117.TXT"

dz = 2.7  # meters

experiment_start = "2026-03-22 15:00:00"
experiment_end   = "2026-03-23 13:00:00"

target_smoothing_minutes = 240

max_lag_hours = 12
peak_height = 1.25
peak_distance = 50

# ==========================================
# PLOTTING STYLE
# ==========================================
plt.rcParams.update({
    "figure.dpi": 300,
    "font.size": 10,
    "axes.labelsize": 11,
    "axes.titlesize": 12
})

# ==========================================
# 3. LOAD FUNCTION
# ==========================================
def load_sensor(file_path):
    df = pd.read_csv(file_path, skiprows=2)
    df['datetime'] = pd.to_datetime(df['time'], unit='s') + pd.to_timedelta(df['millis'], unit='ms')
    return df.set_index('datetime').sort_index()['backscatter']

upper = load_sensor(file_upper)
lower = load_sensor(file_lower)

# ==========================================
# 4. SYNCHRONIZE
# ==========================================
df = pd.merge_asof(
    upper.to_frame("upper"),
    lower.to_frame("lower"),
    left_index=True,
    right_index=True,
    direction="nearest",
    tolerance=pd.Timedelta("1s")
).dropna()

# ==========================================
# 5. FILTER TIME
# ==========================================
df = df.loc[pd.to_datetime(experiment_start):pd.to_datetime(experiment_end)]

# ==========================================
# 6. RAW SIGNAL PLOT
# ==========================================
plt.figure(figsize=(8,4))
plt.plot(df.index, df["upper"], linewidth=0.8, label="Upper raw")
plt.plot(df.index, df["lower"], linewidth=0.8, label="Lower raw")
plt.xlabel("Time")
plt.ylabel("Backscatter (instrument units)")
plt.title("Raw turbidity signals")
plt.legend()
plt.tight_layout()
plt.show()

# ==========================================
# 7. SMOOTHING
# ==========================================
dt_seconds = (df.index[1] - df.index[0]).total_seconds()
window_length = int((target_smoothing_minutes*60)/dt_seconds)

if window_length % 2 == 0:
    window_length += 1

upper_smooth = savgol_filter(df["upper"], window_length, 2)
lower_smooth = savgol_filter(df["lower"], window_length, 2)

# ==========================================
# 8. RAW vs SMOOTHED
# ==========================================
plt.figure(figsize=(8,4))
plt.plot(df.index, df["upper"], linewidth=0.5, alpha=0.5, label="Raw")
plt.plot(df.index, upper_smooth, linewidth=1.2, label="Smoothed")
plt.xlabel("Time")
plt.ylabel("Backscatter")
plt.title("Effect of smoothing (upper sensor)")
plt.legend()
plt.tight_layout()
plt.show()

# ==========================================
# 9. PREPROCESS FOR CORRELATION
# ==========================================
u = detrend(upper_smooth)
l = detrend(lower_smooth)

u = (u - np.mean(u)) / np.std(u)
l = (l - np.mean(l)) / np.std(l)

# ==========================================
# 10. PROCESSED SIGNAL PLOT
# ==========================================
plt.figure(figsize=(8,4))
plt.plot(df.index, u, linewidth=1, label="Upper processed")
plt.plot(df.index, l, linewidth=1, label="Lower processed")
plt.xlabel("Time")
plt.ylabel("Normalized signal")
plt.title("Signals used in correlation")
plt.legend()
plt.tight_layout()
plt.show()

# ==========================================
# 11. PEAK DETECTION
# ==========================================
peaks_u, _ = find_peaks(u, height=peak_height, distance=peak_distance)
peaks_l, _ = find_peaks(l, height=peak_height, distance=peak_distance)

plt.figure(figsize=(8,4))
plt.plot(df.index, u, linewidth=1, label="Upper")
plt.plot(df.index[peaks_u], u[peaks_u], "ro", label="Upper peaks")

plt.plot(df.index, l, linewidth=1, label="Lower")
plt.plot(df.index[peaks_l], l[peaks_l], "go", label="Lower peaks")

plt.xlabel("Time")
plt.ylabel("Normalized signal")
plt.title("Detected peaks used for lag estimation")
plt.legend()
plt.tight_layout()
plt.show()

# ==========================================
# 12. CROSS-CORRELATION
# ==========================================
corr = correlate(l, u, mode='full')
corr = corr / (len(u) * np.std(u) * np.std(l))  # normalize

lags = np.arange(-len(u)+1, len(u)) * dt_seconds

mask = (lags >= -max_lag_hours*3600) & (lags <= max_lag_hours*3600)

lags_limited = lags[mask]
corr_limited = corr[mask]

# Only positive lags
positive_mask = lags_limited > 0
lag_sec = lags_limited[positive_mask][np.argmax(corr_limited[positive_mask])]

velocity = dz / (lag_sec/(24*3600))

# ==========================================
# 13. CORRELATION PLOT
# ==========================================
plt.figure(figsize=(6,4))
plt.plot(lags_limited/3600, corr_limited, linewidth=1.2)
plt.axvline(lag_sec/3600, linestyle='--')
plt.xlabel("Lag time (hours)")
plt.ylabel("Normalized cross-correlation")
plt.title("Lag determination via cross-correlation")
plt.tight_layout()
plt.show()

# ==========================================
# 14. UNCERTAINTY
# ==========================================
peak_val = np.max(corr_limited)
threshold = 0.9 * peak_val

indices = np.where(corr_limited >= threshold)[0]

if len(indices) > 1:
    lag_width_sec = lags_limited[indices[-1]] - lags_limited[indices[0]]
    sigma_t = lag_width_sec / 2
else:
    sigma_t = np.nan

sigma_v = (dz / lag_sec**2) * sigma_t
sigma_v_day = sigma_v * (24*3600)

# ==========================================
# FINAL OUTPUT
# ==========================================
print("\n===== FINAL RESULT =====")
print(f"Lag: {lag_sec/3600:.2f} hours")
print(f"Velocity: {velocity:.2f} ± {sigma_v_day:.2f} m/day")
