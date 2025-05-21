import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D
import numpy as np

# Data (n, t, average runtime)
data = [
    [20000, 2, 0.577255],
    [20000, 4, 0.296651],
    [20000, 8, 0.293474],
    [20000, 16, 0.290521],
    [25000, 2, 0.920419],
    [25000, 4, 0.489994],
    [25000, 8, 0.470956],
    [25000, 16, 0.450763],
    [30000, 2, 1.324529],
    [30000, 4, 0.659446],
    [30000, 8, 0.656013],
    [30000, 16, 0.651398],
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
ax.set_title('Runtime Trends over Data Size and Threads for the Slave')

# Optional: Better viewing angle
ax.view_init(elev=20, azim=-45)

plt.show()
