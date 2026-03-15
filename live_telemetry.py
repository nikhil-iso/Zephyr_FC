import serial
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
from collections import deque

# -------------------------------------------------
# User settings
# -------------------------------------------------
SERIAL_PORT = "COM5"
BAUD_RATE = 115200
MAX_POINTS = 400  # number of points shown on screen

# -------------------------------------------------
# Storage
# -------------------------------------------------
time_data = deque(maxlen=MAX_POINTS)
alt_data = deque(maxlen=MAX_POINTS)
max_alt_data = deque(maxlen=MAX_POINTS)

apogee_time = None
apogee_alt = None

# -------------------------------------------------
# Open serial port
# -------------------------------------------------
ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)

# Skip lines until header is found
while True:
    line = ser.readline().decode(errors="ignore").strip()
    if line.startswith("timestamp_ms"):
        print("Header found:", line)
        break

# -------------------------------------------------
# Plot setup
# -------------------------------------------------
fig, ax = plt.subplots()
line_alt, = ax.plot([], [], label="Altitude (m)")
point_apogee, = ax.plot([], [], "o", label="Apogee")

ax.set_title("Live Altitude Telemetry")
ax.set_xlabel("Time (s)")
ax.set_ylabel("Altitude (m)")
ax.legend()
ax.grid(True)

# -------------------------------------------------
# Animation update function
# -------------------------------------------------
def update(frame):
    global apogee_time, apogee_alt

    # Read as many available lines as possible each update
    for _ in range(10):
        line = ser.readline().decode(errors="ignore").strip()
        if not line:
            continue

        parts = line.split(",")
        if len(parts) != 10:
            continue

        try:
            timestamp_ms = int(parts[0])
            state = int(parts[1])
            altitude_m = float(parts[2])
            current_max_altitude_m = float(parts[9])
        except ValueError:
            continue

        t_s = timestamp_ms / 1000.0

        time_data.append(t_s)
        alt_data.append(altitude_m)
        max_alt_data.append(current_max_altitude_m)

        # Track apogee from the logged max altitude field
        if apogee_alt is None or current_max_altitude_m > apogee_alt:
            apogee_alt = current_max_altitude_m
            apogee_time = t_s

    if len(time_data) > 0:
        line_alt.set_data(time_data, alt_data)
        ax.set_xlim(min(time_data), max(time_data) + 0.1)

        y_min = min(alt_data) - 0.5
        y_max = max(alt_data) + 0.5
        if y_min == y_max:
            y_max = y_min + 1
        ax.set_ylim(y_min, y_max)

        if apogee_time is not None and apogee_alt is not None:
            point_apogee.set_data([apogee_time], [apogee_alt])

    return line_alt, point_apogee

ani = FuncAnimation(fig, update, interval=100, blit=False)
plt.show()

ser.close()