#!/usr/bin/env python3
"""
gen_sun.py - Generates textures/sun.png (512x512 RGBA)
A radiant glowing golden sun with corona rays and soft halo, matching the Whisk3D app logo.
"""

import os
import math
import numpy as np
from PIL import Image, ImageFilter

def generate_sun(output_path, size=512):
    y, x = np.ogrid[:size, :size]
    cx = (size - 1) / 2.0
    cy = (size - 1) / 2.0
    dx = (x - cx) / (size / 2.0)
    dy = (y - cy) / (size / 2.0)
    r = np.sqrt(dx * dx + dy * dy)
    theta = np.arctan2(dy, dx)

    # 1. Multi-frequency corona rays / sunburst beams
    rays1 = 0.5 + 0.5 * np.cos(12 * theta)
    rays2 = 0.5 + 0.5 * np.cos(20 * theta + 0.35)
    rays3 = 0.5 + 0.5 * np.cos(32 * theta + 1.1)
    rays4 = 0.5 + 0.5 * np.cos(6 * theta - 0.2)
    rays_comb = (0.45 * rays1 + 0.30 * rays2 + 0.15 * rays3 + 0.10 * rays4) ** 1.6

    # Radial envelope for rays
    ray_envelope = np.clip((1.0 - r) / 0.88, 0.0, 1.0) ** 1.2
    ray_mask = rays_comb * ray_envelope * (1.0 - np.exp(-10.0 * r))

    # 2. Central cross star glint (subtle, golden-white)
    glint_h = np.exp(-((dy / 0.04) ** 2) - ((dx / 0.7) ** 2))
    glint_v = np.exp(-((dx / 0.04) ** 2) - ((dy / 0.7) ** 2))
    glint_d1 = np.exp(-(((dx + dy) / 0.07) ** 2) - (((dx - dy) / 0.55) ** 2)) * 0.4
    glint_d2 = np.exp(-(((dx - dy) / 0.07) ** 2) - (((dx + dy) / 0.55) ** 2)) * 0.4
    glint = np.clip(glint_h + glint_v + glint_d1 + glint_d2, 0.0, 1.0)

    # 3. Radial core and halos
    core = np.exp(-((r / 0.18) ** 2.2))
    inner_corona = np.exp(-((r / 0.38) ** 1.7))
    outer_halo = np.clip((1.0 - r) / 0.95, 0.0, 1.0) ** 1.8

    # Total intensity
    total_intensity = (
        1.9 * core +
        0.9 * inner_corona +
        0.55 * ray_mask +
        0.6 * glint +
        0.4 * outer_halo
    )

    # Rich Golden Sun Color Palette (matching app logo):
    # Core: #ffffff -> #fffbe6
    # Inner: #ffea3d -> #ffc107
    # Outer: #ffa000 -> #ff8f00
    # Corona rim: #ff7000 (warm golden-amber, not red)
    r_val = np.full_like(r, 255.0)
    
    # G channel: stays rich golden yellow even at outer edges
    g_val = 255 * (
        0.98 * np.exp(-((r / 0.25) ** 1.4)) +
        0.80 * np.exp(-((r / 0.55) ** 1.6)) +
        0.65 * ray_mask * 0.8
    )
    g_val = np.clip(g_val, 150.0, 255.0)

    # B channel: intense in center for pure white, fades to deep gold
    b_val = 255 * (
        0.96 * np.exp(-((r / 0.14) ** 2.2)) +
        0.40 * np.exp(-((r / 0.28) ** 1.8)) +
        0.50 * glint * (1.0 - r)
    )
    b_val = np.clip(b_val, 0.0, 255.0)

    # Alpha: smooth radial fade to zero at perimeter
    alpha = np.clip(
        1.0 * core +
        0.85 * inner_corona +
        0.60 * ray_mask +
        0.70 * glint +
        0.35 * outer_halo,
        0.0, 1.0
    )
    boundary_taper = np.clip((1.0 - r) / 0.12, 0.0, 1.0) ** 1.2
    alpha_arr = np.clip(alpha * boundary_taper * 255, 0, 255).astype(np.uint8)

    rgba = np.zeros((size, size, 4), dtype=np.uint8)
    rgba[:, :, 0] = np.clip(r_val, 0, 255).astype(np.uint8)
    rgba[:, :, 1] = np.clip(g_val, 0, 255).astype(np.uint8)
    rgba[:, :, 2] = np.clip(b_val, 0, 255).astype(np.uint8)
    rgba[:, :, 3] = alpha_arr

    img = Image.fromarray(rgba, 'RGBA')
    bloom = img.filter(ImageFilter.GaussianBlur(radius=6))
    img = Image.alpha_composite(img, Image.blend(img, bloom, 0.25))

    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    img.save(output_path, 'PNG', optimize=True)
    print(f"Generated {output_path} ({size}x{size})")

if __name__ == "__main__":
    generate_sun("/home/danielpdiamon/Whisk3D-Android/app/src/main/assets/textures/sun.png")
