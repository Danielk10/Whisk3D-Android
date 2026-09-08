#!/usr/bin/env python3
"""
gen_sea.py - Generates textures/sea.png (512x512 RGBA)
Seamless repeating vibrant tropical ocean with wave caustics and sun sparkle specks,
matching the Whisk3D app logo.
"""

import os
import math
import numpy as np
from PIL import Image

def generate_sea(output_path, size=512):
    y, x = np.mgrid[:size, :size].astype(np.float32)
    u = (x / float(size)) * 2.0 * math.pi
    v = (y / float(size)) * 2.0 * math.pi

    # 1. Base tropical ocean color gradient & periodic swell waves
    wave1 = np.sin(2.0 * u + 3.0 * v + 0.5)
    wave2 = np.sin(4.0 * u - 2.0 * v + 1.2) * 0.6
    wave3 = np.cos(5.0 * u + 4.0 * v + 2.1) * 0.35
    wave4 = np.sin(7.0 * u - 6.0 * v + 0.8) * 0.2
    wave_field = (wave1 + wave2 + wave3 + wave4) / 2.15

    # Secondary turquoise current ribbons
    current_ribbon = np.sin(1.0 * u + 2.0 * v) * 0.5 + np.cos(2.0 * u - 1.0 * v + 0.4) * 0.5

    # 2. Toroidal Periodic Voronoi Water Caustics with wave warp
    grid_cells = 11
    cell_size = size / float(grid_cells)
    np.random.seed(42)

    offsets_x = (np.random.rand(grid_cells, grid_cells) * 0.65 + 0.18) * cell_size
    offsets_y = (np.random.rand(grid_cells, grid_cells) * 0.65 + 0.18) * cell_size

    # Warp coordinates slightly with periodic waves for natural fluid curves
    warp_amp = 6.5
    wx = x + warp_amp * np.sin(2.0 * u + 1.0 * v)
    wy = y + warp_amp * np.cos(1.0 * u + 3.0 * v)

    cx_idx = ((wx % size) / cell_size).astype(int)
    cy_idx = ((wy % size) / cell_size).astype(int)

    d1_map = np.full((size, size), 9999.0, dtype=np.float32)
    d2_map = np.full((size, size), 9999.0, dtype=np.float32)

    for di in [-1, 0, 1]:
        for dj in [-1, 0, 1]:
            ni = (cx_idx + di) % grid_cells
            nj = (cy_idx + dj) % grid_cells
            pt_x = ni * cell_size + offsets_x[ni, nj]
            pt_y = nj * cell_size + offsets_y[ni, nj]
            dx = np.abs((wx % size) - pt_x)
            dx = np.minimum(dx, size - dx)
            dy = np.abs((wy % size) - pt_y)
            dy = np.minimum(dy, size - dy)
            d = np.sqrt(dx * dx + dy * dy)

            is_smaller = d < d1_map
            d2_map = np.where(is_smaller, d1_map, np.minimum(d2_map, d))
            d1_map = np.where(is_smaller, d, d1_map)

    # Soft, organic caustic line network
    caustic_ridge = np.exp(-((d2_map - d1_map) / 3.4) ** 2)
    caustic_interior = np.clip(1.0 - (d1_map / (cell_size * 0.8)), 0.0, 1.0) ** 1.8
    caustic_net = caustic_ridge * 0.82 + caustic_interior * 0.28

    # 3. Periodic Sun Sparkling Specks
    np.random.seed(2024)
    num_sparkles = 24
    sp_x = np.random.rand(num_sparkles) * size
    sp_y = np.random.rand(num_sparkles) * size
    sp_intensity = np.random.rand(num_sparkles) * 0.5 + 0.5

    sparkle_map = np.zeros((size, size), dtype=np.float32)
    for i in range(num_sparkles):
        sx, sy, sint = sp_x[i], sp_y[i], sp_intensity[i]
        dx = np.abs(x - sx)
        dx = np.minimum(dx, size - dx)
        dy = np.abs(y - sy)
        dy = np.minimum(dy, size - dy)
        r_sp = np.sqrt(dx * dx + dy * dy)
        
        glint_h = np.exp(-((dy / 1.1) ** 2) - ((dx / 7.0) ** 2))
        glint_v = np.exp(-((dx / 1.1) ** 2) - ((dy / 7.0) ** 2))
        core_sp = np.exp(-((r_sp / 1.8) ** 2))
        sp_shape = np.clip(core_sp * 1.6 + glint_h * 0.85 + glint_v * 0.85, 0.0, 2.0)
        sparkle_map += sp_shape * sint

    sparkle_map = np.clip(sparkle_map, 0.0, 1.5)

    # 4. Color Compositing (matching app logo palette)
    # Deep Sapphire: #0c3e78 -> (12, 62, 120)
    # Mid Azure: #1462ad -> (20, 98, 173)
    # Vibrant Tropical Turquoise: #00b4d8 -> (0, 180, 216)
    # Shallow Cyan: #48cae4 -> (72, 202, 228)
    # Caustic Web: #caf0f8 -> (202, 240, 248)
    # Sun sparkle: #ffffff -> (255, 255, 255)
    c_deep = np.array([12, 62, 120], dtype=np.float32)
    c_mid = np.array([20, 98, 173], dtype=np.float32)
    c_turq = np.array([0, 180, 216], dtype=np.float32)
    c_cyan = np.array([72, 202, 228], dtype=np.float32)
    c_caustic = np.array([215, 246, 255], dtype=np.float32)
    c_white = np.array([255, 255, 255], dtype=np.float32)

    t_swell = np.clip((wave_field + 1.0) * 0.5, 0.0, 1.0)
    t_curr = np.clip((current_ribbon + 1.0) * 0.5, 0.0, 1.0)

    rgb = np.zeros((size, size, 3), dtype=np.float32)
    for c in range(3):
        base = c_deep[c] * (1.0 - t_swell) + c_mid[c] * t_swell
        # Tropical currents
        base = base * (1.0 - 0.45 * t_curr) + c_turq[c] * (0.45 * t_curr)
        # Wave crest highlights
        crest = np.clip(wave_field - 0.45, 0.0, 0.55) / 0.55
        base = base * (1.0 - 0.35 * crest) + c_cyan[c] * (0.35 * crest)
        # Caustics
        caust = np.clip(caustic_net, 0.0, 1.0)
        base = base * (1.0 - 0.42 * caust) + c_caustic[c] * (0.42 * caust)
        # Sparkles
        spk = np.clip(sparkle_map, 0.0, 1.0)
        base = base * (1.0 - spk) + c_white[c] * spk
        rgb[:, :, c] = base

    rgba = np.zeros((size, size, 4), dtype=np.uint8)
    rgba[:, :, :3] = np.clip(rgb, 0, 255).astype(np.uint8)
    rgba[:, :, 3] = 255

    img = Image.fromarray(rgba, 'RGBA')
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    img.save(output_path, 'PNG', optimize=True)
    print(f"Generated {output_path} ({size}x{size})")

if __name__ == "__main__":
    generate_sea("/home/danielpdiamon/Whisk3D-Android/app/src/main/assets/textures/sea.png")
