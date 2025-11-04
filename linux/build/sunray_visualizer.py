import matplotlib.pyplot as plt
import matplotlib.animation as animation
import numpy as np
import json, os

# --- Settings ---
width, height = 10.0, 10.0
res = 0.1
cols, rows = int(width / res), int(height / res)
grass = np.ones((rows, cols)) * 5.0  # default height (5mm)
cut_height = 2.0

obstacles = np.array([[4, 4], [6, 2], [2, 7]])
dock = np.array([0.5, 0.5])

# --- Visualization Setup ---
fig, ax = plt.subplots(figsize=(6, 6))
ax.set_xlim(0, width)
ax.set_ylim(0, height)
ax.set_title("Sunray Smart Lawn Mower — Row-by-Row Simulation", fontsize=12)

img = ax.imshow(grass, cmap="YlGn", origin="lower",
                extent=[0, width, 0, height], vmin=0, vmax=5)
mower_dot, = ax.plot([], [], 'ro', markersize=7)
ax.scatter(obstacles[:, 0], obstacles[:, 1], c='black', s=60, marker='s', label="Obstacle")
ax.scatter(dock[0], dock[1], c='yellow', s=100, marker='+', label="Dock")
info = ax.text(0.02, 1.02, "", transform=ax.transAxes, fontsize=9, va="bottom")
ax.legend(loc="upper right")
plt.tight_layout()

# --- Read Live Data ---
def read_position():
    try:
        with open("position.txt") as f:
            vals = f.read().strip().split(",")
            if len(vals) >= 4:
                return float(vals[0]), float(vals[1]), float(vals[2]), vals[3]
    except Exception:
        pass
    return None

# --- Update Animation ---
def update(_):
    pos = read_position()
    if not pos:
        return [img, mower_dot, info]

    x, y, bat, state = pos
    gx, gy = int(x / res), int(y / res)
    gx, gy = np.clip(gx, 0, cols - 1), np.clip(gy, 0, rows - 1)

    # mow around current position
    mow_r = 3
    grass[max(0, gy - mow_r):min(rows, gy + mow_r),
          max(0, gx - mow_r):min(cols, gx + mow_r)] = cut_height

    mower_dot.set_data(x, y)
    img.set_data(grass)
    info.set_text(f"Battery: {bat:.1f}% | State: {state}")
    return [img, mower_dot, info]

ani = animation.FuncAnimation(fig, update, interval=200)
plt.show()
