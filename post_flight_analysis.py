import pandas as pd
import matplotlib.pyplot as plt

# -------------------------------------------------
# Load data
# -------------------------------------------------
df = pd.read_csv("flight_log.csv")

# Convert time to seconds
df["time_s"] = df["timestamp_ms"] / 1000.0

# -------------------------------------------------
# Find the actual flight window
# state: 0 = IDLE, 1 = ASCENT, 2 = DESCENT
# -------------------------------------------------
flight_df = df[df["state"] != 0].copy()

if len(flight_df) == 0:
    print("No flight data found.")
    raise SystemExit

# -------------------------------------------------
# Metrics
# -------------------------------------------------
apogee_index = flight_df["current_max_altitude_m"].idxmax()
apogee_altitude = flight_df.loc[apogee_index, "current_max_altitude_m"]
apogee_time = flight_df.loc[apogee_index, "time_s"] - flight_df["time_s"].iloc[0]

total_flight_time = flight_df["time_s"].iloc[-1] - flight_df["time_s"].iloc[0]

print(f"Apogee altitude: {apogee_altitude:.3f} m")
print(f"Time to apogee: {apogee_time:.3f} s")
print(f"Total flight time: {total_flight_time:.3f} s")

# -------------------------------------------------
# Plot altitude profile
# -------------------------------------------------
plt.figure()
plt.plot(flight_df["time_s"], flight_df["altitude_m"], label="Altitude")
plt.scatter(flight_df.loc[apogee_index, "time_s"],
            flight_df.loc[apogee_index, "current_max_altitude_m"],
            label="Apogee")

plt.title("Altitude vs Time")
plt.xlabel("Time (s)")
plt.ylabel("Altitude (m)")
plt.grid(True)
plt.legend()
plt.show()