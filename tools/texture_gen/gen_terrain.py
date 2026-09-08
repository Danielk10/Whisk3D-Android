#!/usr/bin/env python3
"""
gen_terrain.py - Generates textures/terrain.png (512x512 RGBA)
Stylized tropical island terrain atlas matching the Whisk3D app logo:
- Top (Y: 0-110): Coastal shallow turquoise sea, white foam wash, golden sandy beach
- Middle (Y: 100-360): Lush tropical jungle with dense rainforest canopy, palm fronds, terraced green layers
- Bottom (Y: 340-512): Low-poly faceted volcanic rock faces, sharp cliff planes, and basalt peak
Horizontal seamless wrap (X=0 connects to X=511).
"""

import os
import math
import numpy as np
from PIL import Image

def generate_terrain(output_path, size=512):
    y, x = np.mgrid[:size, :size].astype(np.float32)
    # Isotropic angular frequencies
    u = (x / float(size)) * 2.0 * math.pi
    v = (y / float(size)) * 2.0 * math.pi
    ny = y / float(size)

    # 1. Natural undulating bay & ridge lines (periodic in X)
    bay_curve = (
        0.045 * np.sin(2.0 * u) +
        0.025 * np.cos(4.0 * u + 0.7) +
        0.012 * np.sin(6.0 * u - 1.2)
    )

    eff_y_beach = ny + bay_curve * 0.7
    eff_y_rock  = ny + bay_curve * 1.3

    # Zone masks with smooth transitions
    m_water = np.clip((0.04 - eff_y_beach) / 0.04, 0.0, 1.0)
    m_foam  = np.exp(-((eff_y_beach - 0.045) / 0.014) ** 2)
    m_beach = np.clip((eff_y_beach - 0.03) / 0.06, 0.0, 1.0) * np.clip((0.26 - eff_y_beach) / 0.08, 0.0, 1.0)
    m_jungle = np.clip((eff_y_beach - 0.16) / 0.08, 0.0, 1.0) * np.clip((0.74 - eff_y_rock) / 0.08, 0.0, 1.0)
    m_rock = np.clip((eff_y_rock - 0.67) / 0.08, 0.0, 1.0)

    # 2. Voronoi Faceted Rock Geometry (Low-Poly Mountain Crags)
    rock_grid_x = 11
    rock_grid_y = 6
    cell_w = size / float(rock_grid_x)
    cell_h = (size * 0.38) / float(rock_grid_y)
    rock_y_start = size * 0.62

    np.random.seed(888)
    rock_pts = []
    rock_normals = []
    for gi in range(rock_grid_x):
        for gj in range(rock_grid_y):
            px = (gi + np.random.rand() * 0.7 + 0.15) * cell_w
            py = rock_y_start + (gj + np.random.rand() * 0.7 + 0.15) * cell_h
            rock_pts.append((px, py))
            fnx = np.random.uniform(-0.85, 0.85)
            fny = np.random.uniform(-0.85, 0.35)
            fnz = np.random.uniform(0.35, 0.95)
            flen = math.sqrt(fnx*fnx + fny*fny + fnz*fnz)
            rock_normals.append((fnx/flen, fny/flen, fnz/flen))

    rock_pts = np.array(rock_pts, dtype=np.float32)
    rock_normals = np.array(rock_normals, dtype=np.float32)

    lx, ly, lz = 0.55, -0.65, 0.52
    l_len = math.sqrt(lx*lx + ly*ly + lz*lz)
    lx, ly, lz = lx/l_len, ly/l_len, lz/l_len

    rock_light_map = np.full((size, size), 0.5, dtype=np.float32)
    in_rock_zone = y >= (size * 0.58)
    y_sub = y[in_rock_zone]
    x_sub = x[in_rock_zone]

    min_d_idx = np.zeros(x_sub.shape, dtype=int)
    min_dist = np.full(x_sub.shape, 99999.0, dtype=np.float32)

    for idx, (px, py) in enumerate(rock_pts):
        pdx = np.abs(x_sub - px)
        pdx = np.minimum(pdx, size - pdx)
        pdy = y_sub - py
        dist_sq = pdx * pdx + pdy * pdy
        is_closer = dist_sq < min_dist
        min_dist = np.where(is_closer, dist_sq, min_dist)
        min_d_idx = np.where(is_closer, idx, min_d_idx)

    facet_normals = rock_normals[min_d_idx]
    facet_ndotl = facet_normals[:, 0] * lx + facet_normals[:, 1] * ly + facet_normals[:, 2] * lz
    facet_light = np.clip(facet_ndotl * 0.6 + 0.45, 0.15, 1.0)
    rock_light_map[in_rock_zone] = facet_light

    # 3. Rich Isotropic Jungle Canopy Field
    canopy_wave = (
        0.35 * np.sin(3.0 * u + 3.0 * v) +
        0.28 * np.cos(5.0 * u - 5.0 * v + 0.8) +
        0.22 * np.sin(8.0 * u + 7.0 * v + 1.5) +
        0.15 * np.cos(12.0 * u - 11.0 * v)
    )
    canopy_sun = np.clip(canopy_wave * 1.7, -1.0, 1.0)

    # 4. Stylized Palm Tree and Jungle Crown Outlines (Scattered in middle zone)
    np.random.seed(555)
    num_trees = 48
    tree_x = np.random.rand(num_trees) * size
    tree_y = np.random.uniform(size * 0.20, size * 0.64, size=num_trees)
    tree_r = np.random.uniform(14.0, 24.0, size=num_trees)

    tree_canopy = np.zeros((size, size), dtype=np.float32)
    tree_shades = np.zeros((size, size), dtype=np.float32)

    for i in range(num_trees):
        tx, ty, tr = tree_x[i], tree_y[i], tree_r[i]
        pdx = np.abs(x - tx)
        pdx = np.minimum(pdx, size - pdx)
        pdy = y - ty
        d = np.sqrt(pdx * pdx + pdy * pdy)
        in_tree = np.clip(1.0 - (d / tr), 0.0, 1.0)
        
        ang = np.arctan2(pdy, pdx)
        fronds = 0.5 + 0.5 * np.cos(6.0 * ang)
        tree_shape = np.clip(in_tree * (0.65 + 0.35 * fronds), 0.0, 1.0)
        tree_sun = np.clip((-pdx * 0.6 - pdy * 0.8) / (tr * 0.65), 0.0, 1.0)
        tree_canopy = np.maximum(tree_canopy, tree_shape * (0.4 + 0.6 * tree_sun))

        shd_dy = y - (ty + tr * 0.45)
        shd_d = np.sqrt(pdx*pdx + shd_dy*shd_dy)
        tree_shd = np.clip(1.0 - shd_d / (tr * 0.8), 0.0, 1.0)
        tree_shades = np.maximum(tree_shades, tree_shd * 0.6)

    # 5. Color Palette
    c_water = np.array([56, 189, 248], dtype=np.float32)
    c_foam  = np.array([245, 252, 255], dtype=np.float32)
    c_sand  = np.array([244, 218, 162], dtype=np.float32)
    c_sand_shd = np.array([218, 186, 128], dtype=np.float32)

    c_grass_sun  = np.array([125, 206, 70], dtype=np.float32)  # #7dce46 sunlit foliage
    c_grass_mid  = np.array([54, 156, 62], dtype=np.float32)   # #369c3e tropical green
    c_jungle_deep= np.array([28, 98, 38], dtype=np.float32)    # #1c6226 deep jungle
    c_jungle_shd = np.array([16, 64, 24], dtype=np.float32)    # #104018 under-canopy shadow

    c_rock_sun  = np.array([150, 164, 180], dtype=np.float32)
    c_rock_mid  = np.array([96, 108, 124], dtype=np.float32)
    c_rock_shd  = np.array([52, 60, 72], dtype=np.float32)
    c_rock_peak = np.array([30, 36, 46], dtype=np.float32)

    rgb = np.zeros((size, size, 3), dtype=np.float32)

    for c in range(3):
        # 1. Beach sand
        sand_val = c_sand[c] * (1.0 - bay_curve * 0.4) + c_sand_shd[c] * (bay_curve * 0.4)

        # 2. Rich isotropic textured jungle
        j_canopy = (
            c_grass_mid[c] * (1.0 - canopy_sun * 0.45) +
            c_grass_sun[c] * np.maximum(0.0, canopy_sun * 0.55) +
            c_jungle_deep[c] * np.maximum(0.0, -canopy_sun * 0.55)
        )
        j_canopy = j_canopy * (1.0 - tree_shades * 0.5) + c_jungle_shd[c] * (tree_shades * 0.5)
        j_canopy = j_canopy * (1.0 - tree_canopy * 0.7) + c_grass_sun[c] * (tree_canopy * 0.7)
        j_prog = np.clip((ny - 0.22) / 0.48, 0.0, 1.0)
        j_final = j_canopy * (1.0 - j_prog * 0.35) + c_jungle_deep[c] * (j_prog * 0.35)

        # 3. Faceted rock
        r_lit = rock_light_map
        rock_val = c_rock_mid[c] * (1.0 - r_lit) + c_rock_sun[c] * r_lit
        peak_prog = np.clip((ny - 0.84) / 0.16, 0.0, 1.0)
        rock_val = rock_val * (1.0 - peak_prog * 0.65) + c_rock_peak[c] * (peak_prog * 0.65)

        # Composite layers
        col = sand_val
        col = col * (1.0 - m_jungle) + j_final * m_jungle
        col = col * (1.0 - m_rock) + rock_val * m_rock

        # Coastline water & white foam
        col = col * (1.0 - m_water) + c_water[c] * m_water
        col = col * (1.0 - m_foam) + c_foam[c] * m_foam

        rgb[:, :, c] = col

    rgba = np.zeros((size, size, 4), dtype=np.uint8)
    rgba[:, :, :3] = np.clip(rgb, 0, 255).astype(np.uint8)
    rgba[:, :, 3] = 255

    diff_x = np.max(np.abs(rgba[:, 0, :3].astype(int) - rgba[:, -1, :3].astype(int)))
    print(f"Terrain horizontal seam check - max delta X: {diff_x}px")

    img = Image.fromarray(rgba, 'RGBA')
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    img.save(output_path, 'PNG', optimize=True)
    print(f"Generated {output_path} ({size}x{size})")

if __name__ == "__main__":
    generate_terrain("/home/danielpdiamon/Whisk3D-Android/app/src/main/assets/textures/terrain.png")
