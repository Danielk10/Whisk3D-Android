#!/usr/bin/env python3
"""
gen_cloud.py - Generates textures/cloud.png (512x512 RGBA)
Stylized, fluffy cumulus clouds with organic billows and smooth alpha gradient,
matching the Whisk3D app logo.
"""

import os
import math
import numpy as np
from PIL import Image

def gaussian_blur_np(arr, sigma=2.0):
    radius = max(1, int(3.0 * sigma))
    x = np.arange(-radius, radius + 1, dtype=np.float32)
    k = np.exp(-0.5 * (x / sigma) ** 2)
    k /= k.sum()
    res = np.apply_along_axis(lambda m: np.convolve(m, k, mode='same'), 0, arr)
    res = np.apply_along_axis(lambda m: np.convolve(m, k, mode='same'), 1, res)
    return res

def generate_cloud(output_path, size=512):
    y, x = np.mgrid[:size, :size].astype(np.float32)
    nx = x / float(size)
    ny = y / float(size)

    # Majestic cumulus mounds scaled to fill ~85% of canvas
    mounds = [
        # (cx, cy, rx, ry, weight)
        (0.50, 0.42, 0.28, 0.22, 1.0),
        (0.38, 0.46, 0.24, 0.19, 0.95),
        (0.62, 0.44, 0.25, 0.20, 0.95),
        (0.52, 0.32, 0.19, 0.17, 0.90),
        (0.28, 0.50, 0.19, 0.16, 0.80),
        (0.72, 0.48, 0.20, 0.17, 0.80),
        (0.18, 0.56, 0.15, 0.12, 0.65),
        (0.82, 0.54, 0.16, 0.13, 0.65),
        (0.10, 0.62, 0.09, 0.07, 0.45),
        (0.90, 0.60, 0.09, 0.07, 0.45),
        (0.50, 0.55, 0.35, 0.13, 0.75),
    ]

    density = np.zeros((size, size), dtype=np.float32)
    for (cx, cy, rx, ry, w) in mounds:
        dx = (nx - cx) / rx
        dy = (ny - cy) / ry
        d2 = dx * dx + dy * dy
        q = np.clip(1.0 - d2, 0.0, 1.0)
        density += (q ** 2) * (3.0 - 2.0 * q) * w

    max_d = density.max()
    if max_d > 0:
        density /= max_d

    # Subtle harmonic perturbations
    pert = (
        0.04 * np.sin(16.0 * nx + 1.2) * np.cos(12.0 * ny) +
        0.03 * np.cos(24.0 * nx - 1.8 * ny) +
        0.02 * np.sin(36.0 * nx + 28.0 * ny)
    )
    cloud_field = density + pert * np.clip(density * (1.0 - density) * 4.0, 0.0, 1.0)
    cloud_field = gaussian_blur_np(cloud_field, sigma=3.0)

    # 3D normal from blurred height field
    h_smooth = gaussian_blur_np(cloud_field, sigma=5.0)
    dh_dy, dh_dx = np.gradient(h_smooth)

    # Light direction: upper-right sun
    lx, ly, lz = 0.55, -0.65, 0.52
    l_len = math.sqrt(lx*lx + ly*ly + lz*lz)
    lx, ly, lz = lx/l_len, ly/l_len, lz/l_len

    nz = 0.032
    norm_x = -dh_dx
    norm_y = -dh_dy
    norm_z = np.full_like(norm_x, nz)
    n_len = np.sqrt(norm_x**2 + norm_y**2 + norm_z**2) + 1e-6
    norm_x /= n_len
    norm_y /= n_len
    norm_z /= n_len

    diffuse = np.clip(norm_x * lx + norm_y * ly + norm_z * lz, 0.0, 1.0)

    # Silhouette alpha: smoothstep
    thresh = 0.18
    alpha = np.clip((cloud_field - thresh) / 0.13, 0.0, 1.0)
    alpha = (alpha ** 1.2) * 255.0

    # Shading
    rim_hl = np.clip((diffuse - 0.38) / 0.52, 0.0, 1.0) ** 1.5
    vert_shade = np.clip((ny - 0.30) / 0.36, 0.0, 1.0)
    shadow = np.clip((0.55 - diffuse) * 1.4 + vert_shade * 0.55, 0.0, 1.0)

    # Colors
    c_rim = np.array([255, 255, 255], dtype=np.float32)
    c_body = np.array([250, 252, 255], dtype=np.float32)
    c_shd = np.array([180, 198, 222], dtype=np.float32)
    c_deep = np.array([135, 154, 184], dtype=np.float32)

    rgb = np.zeros((size, size, 3), dtype=np.float32)
    for c in range(3):
        col = c_body[c] * (1.0 - shadow) + c_shd[c] * shadow
        col = col * (1.0 - 0.3 * (shadow ** 2)) + c_deep[c] * (0.3 * (shadow ** 2))
        col = col * (1.0 - rim_hl) + c_rim[c] * rim_hl
        rgb[:, :, c] = col

    border_dist = np.minimum(np.minimum(x, size - 1 - x), np.minimum(y, size - 1 - y))
    border_fade = np.clip(border_dist / 12.0, 0.0, 1.0)
    alpha_final = np.clip(alpha * border_fade, 0, 255).astype(np.uint8)

    rgba = np.zeros((size, size, 4), dtype=np.uint8)
    rgba[:, :, :3] = np.clip(rgb, 0, 255).astype(np.uint8)
    rgba[:, :, 3] = alpha_final

    img = Image.fromarray(rgba, 'RGBA')
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    img.save(output_path, 'PNG', optimize=True)
    print(f"Generated {output_path} ({size}x{size})")

if __name__ == "__main__":
    generate_cloud("/home/danielpdiamon/Whisk3D-Android/app/src/main/assets/textures/cloud.png")
