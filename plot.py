import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D
import numpy as np

# Data (n, t, average runtime)
data = [
    [20000, 2, 78.539681],
    [20000, 4, 64.164683],
    [20000, 8, 55.379865],
    [20000, 16, 55.277927],
    [25000, 2, 122.330378],
    [25000, 4, 100.341758],
    [25000, 8, 86.407713],
    [25000, 16, 85.870184],
    [30000, 2, 178.102681],
    [30000, 4, 144.264106],
    [30000, 8, 124.376693],
    [30000, 16, 124.021209],
]

# Convert to NumPy arrays
n_vals = np.array([row[0] for row in data])
t_vals = np.array([row[1] for row in data])
z_vals = np.array([row[2] for row in data])

# Create 3D plot
fig = plt.figure()
ax = fig.add_subplot(111, projection='3d')

# Surface plot using trisurf (works with scattered non-grid data)
surface = ax.plot_trisurf(t_vals, n_vals, z_vals, cmap='viridis', edgecolor='none')
ax.scatter(t_vals, n_vals, z_vals, color='red', s=40, label='Data Points')


# Labels
ax.set_xlabel('t (Threads)')
ax.set_ylabel('n (Data Size)')
ax.set_zlabel('Average Runtime (seconds)')
# ax.set_title('Runtime Trends over Data Size and Threads for the Slave')

# Optional: Better viewing angle
ax.view_init(elev=20, azim=-45)

plt.show()
