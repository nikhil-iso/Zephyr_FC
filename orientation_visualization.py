import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
from mpl_toolkits.mplot3d.art3d import Poly3DCollection

# -------------------------------------------------
# Load data
# -------------------------------------------------
df = pd.read_csv("flight_log.csv")
df = df[df["state"] != 0].copy()

if len(df) == 0:
    print("No flight data found.")
    raise SystemExit

df["time_s"] = df["timestamp_ms"] / 1000.0

# -------------------------------------------------
# Cube geometry
# -------------------------------------------------
cube_vertices = np.array([
    [-0.5, -0.5, -0.5],
    [ 0.5, -0.5, -0.5],
    [ 0.5,  0.5, -0.5],
    [-0.5,  0.5, -0.5],
    [-0.5, -0.5,  0.5],
    [ 0.5, -0.5,  0.5],
    [ 0.5,  0.5,  0.5],
    [-0.5,  0.5,  0.5]
])

cube_faces = [
    [0, 1, 2, 3],
    [4, 5, 6, 7],
    [0, 1, 5, 4],
    [2, 3, 7, 6],
    [1, 2, 6, 5],
    [0, 3, 7, 4]
]

# -------------------------------------------------
# Rotation matrices
# -------------------------------------------------
def rot_x(angle_rad):
    c = np.cos(angle_rad)
    s = np.sin(angle_rad)
    return np.array([
        [1, 0, 0],
        [0, c, -s],
        [0, s,  c]
    ])

def rot_y(angle_rad):
    c = np.cos(angle_rad)
    s = np.sin(angle_rad)
    return np.array([
        [ c, 0, s],
        [ 0, 1, 0],
        [-s, 0, c]
    ])

# -------------------------------------------------
# Figure setup
# -------------------------------------------------
fig = plt.figure()
ax = fig.add_subplot(111, projection="3d")

max_alt = max(1.0, df["altitude_m"].max() + 1.0)

ax.set_xlim(-2, 2)
ax.set_ylim(-2, 2)
ax.set_zlim(-1, max_alt)
ax.set_xlabel("X")
ax.set_ylabel("Y")
ax.set_zlabel("Altitude")
ax.set_title("Flight Orientation Visualization")

poly = Poly3DCollection([], alpha=0.6, edgecolor="k")
ax.add_collection3d(poly)

# -------------------------------------------------
# Animation update
# -------------------------------------------------
def update(frame):
    ax.collections.clear()

    pitch_deg = df.iloc[frame]["pitch_deg"]
    roll_deg = df.iloc[frame]["roll_deg"]
    altitude = df.iloc[frame]["altitude_m"]

    pitch_rad = np.radians(pitch_deg)
    roll_rad = np.radians(roll_deg)

    # Apply pitch then roll
    R = rot_y(pitch_rad) @ rot_x(roll_rad)

    rotated = (R @ cube_vertices.T).T
    rotated[:, 2] += altitude

    faces = [[rotated[idx] for idx in face] for face in cube_faces]

    poly = Poly3DCollection(faces, alpha=0.6, edgecolor="k")
    ax.add_collection3d(poly)

    ax.set_xlim(-2, 2)
    ax.set_ylim(-2, 2)
    ax.set_zlim(-1, max_alt)
    ax.set_xlabel("X")
    ax.set_ylabel("Y")
    ax.set_zlabel("Altitude")
    ax.set_title(f"Flight Orientation Visualization   t = {df.iloc[frame]['time_s']:.2f} s")

ani = FuncAnimation(fig, update, frames=len(df), interval=50, repeat=False)
plt.show()