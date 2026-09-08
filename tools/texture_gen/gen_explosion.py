#!/usr/bin/env python3
"""
gen_explosion.py - Generates textures/fx_explosion.png (512x512 RGBA)
Multi-element & 4-frame animated fiery explosion matching the Whisk3D app logo:
- Frame 0 (Top-Left): Ignition flash & starburst diffraction rays
- Frame 1 (Top-Right): Peak fiery fireball detonation with boiling flame lobes & incandescent core
- Frame 2 (Bottom-Left): Expanding billowing fire & dark charcoal smoke clouds with fiery rim light
- Frame 3 (Bottom-Right): Dissipating volumetric smoke puffs with floating glowing embers
"""

import os
import math
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

def render_frame_0(size=256):
    """Ignition flash and diffraction starburst"""
    y, x = np.ogrid[:size, :size]
    cx, cy = size / 2.0, size / 2.0
    dx = (x - cx) / (size / 2.0)
    dy = (y - cy) / (size / 2.0)
    r = np.sqrt(dx*dx + dy*dy)

    # 8-Point Diffraction Starburst
    star_8 = (
        np.exp(-((dy / 0.035)**2) - ((dx / 0.8)**2)) +
        np.exp(-((dx / 0.035)**2) - ((dy / 0.8)**2)) +
        np.exp(-(((dx + dy) / 0.05)**2) - (((dx - dy) / 0.65)**2)) * 0.7 +
        np.exp(-(((dx - dy) / 0.05)**2) - (((dx + dy) / 0.65)**2)) * 0.7
    )
    star_8 = np.clip(star_8, 0.0, 1.5)

    core = np.exp(-((r / 0.18)**2.2))
    inner_fire = np.exp(-((r / 0.38)**2.0))
    shock_ring = np.exp(-(((r - 0.48) / 0.08)**2)) * 0.6

    intensity = 1.8 * core + 0.9 * inner_fire + 0.7 * star_8 + 0.5 * shock_ring

    r_ch = np.full_like(r, 255.0)
    g_ch = 255.0 * (0.95 * np.exp(-((r / 0.22)**1.6)) + 0.65 * np.exp(-((r / 0.45)**1.8)))
    b_ch = 255.0 * (0.95 * np.exp(-((r / 0.12)**2.5)) + 0.40 * star_8)

    alpha = np.clip(intensity * 1.3, 0.0, 1.0)
    border_fade = np.clip((1.0 - r) / 0.15, 0.0, 1.0)
    alpha = np.clip(alpha * border_fade * 255.0, 0, 255).astype(np.uint8)

    rgba = np.zeros((size, size, 4), dtype=np.uint8)
    rgba[:, :, 0] = np.clip(r_ch, 0, 255).astype(np.uint8)
    rgba[:, :, 1] = np.clip(g_ch, 0, 255).astype(np.uint8)
    rgba[:, :, 2] = np.clip(b_ch, 0, 255).astype(np.uint8)
    rgba[:, :, 3] = alpha

    f_img = Image.fromarray(rgba, 'RGBA')
    bloom = f_img.filter(ImageFilter.GaussianBlur(radius=3))
    return Image.alpha_composite(f_img, Image.blend(f_img, bloom, 0.3))

def render_frame_1(size=256):
    """Peak fiery fireball explosion (matches app logo)"""
    y, x = np.mgrid[:size, :size].astype(np.float32)
    cx, cy = size / 2.0, size / 2.0
    dx = (x - cx) / (size / 2.0)
    dy = (y - cy) / (size / 2.0)

    # Overlapping billowing spherical fire lobes
    lobes = [
        # (cx, cy, r, weight)
        (0.00,  0.00, 0.42, 1.0),
        (-0.24, -0.18, 0.32, 0.9),
        ( 0.26, -0.16, 0.34, 0.9),
        (-0.28,  0.22, 0.30, 0.85),
        ( 0.25,  0.24, 0.32, 0.85),
        ( 0.00, -0.38, 0.28, 0.8),
        ( 0.00,  0.36, 0.28, 0.8),
        (-0.38,  0.00, 0.26, 0.75),
        ( 0.38,  0.00, 0.26, 0.75),
    ]

    fire_density = np.zeros((size, size), dtype=np.float32)
    for (lcx, lcy, lr, lw) in lobes:
        ldx = (dx - lcx) / lr
        ldy = (dy - lcy) / lr
        dist_sq = ldx * ldx + ldy * ldy
        q = np.clip(1.0 - dist_sq, 0.0, 1.0)
        fire_density += (q ** 2) * (3.0 - 2.0 * q) * lw

    max_d = fire_density.max()
    if max_d > 0:
        fire_density /= max_d

    r = np.sqrt(dx*dx + dy*dy)

    # 3-Zone Thermal Color Mapping
    # Core (density > 0.65): Incandescent white/gold #ffffff -> #ffee55
    # Mantle (0.35 < density <= 0.65): Fiery orange #ff7900 -> #ff9100
    # Outer crust (density <= 0.35): Fiery scarlet #e63946 -> #b91c1c
    c_white = np.array([255, 255, 255], dtype=np.float32)
    c_gold  = np.array([255, 225, 60], dtype=np.float32)
    c_orange= np.array([255, 110, 10], dtype=np.float32)
    c_red   = np.array([200, 20, 10], dtype=np.float32)

    rgb = np.zeros((size, size, 3), dtype=np.float32)
    for c in range(3):
        # Blend red to orange
        t1 = np.clip(fire_density / 0.38, 0.0, 1.0)
        col1 = c_red[c] * (1.0 - t1) + c_orange[c] * t1
        # Blend orange to gold
        t2 = np.clip((fire_density - 0.38) / 0.32, 0.0, 1.0)
        col2 = col1 * (1.0 - t2) + c_gold[c] * t2
        # Blend gold to incandescent white
        t3 = np.clip((fire_density - 0.70) / 0.30, 0.0, 1.0)
        rgb[:, :, c] = col2 * (1.0 - t3) + c_white[c] * t3

    # Alpha silhouette
    alpha = np.clip(fire_density / 0.12, 0.0, 1.0) * 255.0
    border_fade = np.clip((0.95 - r) / 0.12, 0.0, 1.0)
    alpha = np.clip(alpha * border_fade, 0, 255).astype(np.uint8)

    rgba = np.zeros((size, size, 4), dtype=np.uint8)
    rgba[:, :, :3] = np.clip(rgb, 0, 255).astype(np.uint8)
    rgba[:, :, 3] = alpha

    f_img = Image.fromarray(rgba, 'RGBA')
    draw = ImageDraw.Draw(f_img)

    # Flying bright burning sparks
    np.random.seed(777)
    num_sparks = 24
    for _ in range(num_sparks):
        ang = np.random.rand() * 2 * math.pi
        sr = np.random.uniform(0.45, 0.90)
        sx = cx + sr * (size / 2.0) * math.cos(ang)
        sy = cy + sr * (size / 2.0) * math.sin(ang)
        s_sz = float(np.random.uniform(1.8, 3.5))
        draw.ellipse([sx - s_sz, sy - s_sz, sx + s_sz, sy + s_sz], fill=(255, 245, 180, 255))

    bloom = f_img.filter(ImageFilter.GaussianBlur(radius=3))
    return Image.alpha_composite(f_img, Image.blend(f_img, bloom, 0.25))

def render_frame_2(size=256):
    """Expanding fire & dark charcoal smoke cloud with fiery rim lighting"""
    y, x = np.mgrid[:size, :size].astype(np.float32)
    cx, cy = size / 2.0, size / 2.0
    dx = (x - cx) / (size / 2.0)
    dy = (y - cy) / (size / 2.0)
    r = np.sqrt(dx*dx + dy*dy)

    # Smoke cloud lobes
    lobes = [
        (0.00,  0.00, 0.46, 1.0),
        (-0.28, -0.22, 0.36, 0.9),
        ( 0.28, -0.20, 0.38, 0.9),
        (-0.30,  0.25, 0.34, 0.85),
        ( 0.28,  0.26, 0.36, 0.85),
        ( 0.00, -0.42, 0.30, 0.8),
        ( 0.00,  0.40, 0.30, 0.8),
    ]

    smoke_density = np.zeros((size, size), dtype=np.float32)
    for (lcx, lcy, lr, lw) in lobes:
        ldx = (dx - lcx) / lr
        ldy = (dy - lcy) / lr
        dist_sq = ldx * ldx + ldy * ldy
        q = np.clip(1.0 - dist_sq, 0.0, 1.0)
        smoke_density += (q ** 2) * (3.0 - 2.0 * q) * lw

    max_d = smoke_density.max()
    if max_d > 0:
        smoke_density /= max_d

    # Fire core in center
    fire_core = np.exp(-((r / 0.26)**2.2))
    # Fiery rim lighting along smoke billow contours
    rim_glow = np.exp(-(((smoke_density - 0.45) / 0.18)**2)) * 0.85

    c_smoke = np.array([28, 36, 48], dtype=np.float32)    # Dark charcoal soot
    c_rim   = np.array([255, 120, 10], dtype=np.float32)  # Molten orange rim
    c_core  = np.array([255, 230, 80], dtype=np.float32)  # Bright yellow flame core

    rgb = np.zeros((size, size, 3), dtype=np.float32)
    for c in range(3):
        col = c_smoke[c]
        col = col * (1.0 - rim_glow) + c_rim[c] * rim_glow
        col = col * (1.0 - fire_core) + c_core[c] * fire_core
        rgb[:, :, c] = col

    alpha = np.clip(smoke_density / 0.10, 0.0, 1.0) * 255.0
    border_fade = np.clip((0.95 - r) / 0.12, 0.0, 1.0)
    alpha = np.clip(alpha * border_fade, 0, 255).astype(np.uint8)

    rgba = np.zeros((size, size, 4), dtype=np.uint8)
    rgba[:, :, :3] = np.clip(rgb, 0, 255).astype(np.uint8)
    rgba[:, :, 3] = alpha

    f_img = Image.fromarray(rgba, 'RGBA')
    draw = ImageDraw.Draw(f_img)

    np.random.seed(999)
    for _ in range(16):
        ang = np.random.rand() * 2 * math.pi
        sr = np.random.uniform(0.35, 0.88)
        sx = cx + sr * (size / 2.0) * math.cos(ang)
        sy = cy + sr * (size / 2.0) * math.sin(ang)
        draw.ellipse([sx - 2, sy - 2, sx + 2, sy + 2], fill=(255, 175, 40, 220))

    return f_img

def render_frame_3(size=256):
    """Dissipating volumetric smoke puffs with floating glowing embers"""
    y, x = np.mgrid[:size, :size].astype(np.float32)
    cx, cy = size / 2.0, size / 2.0
    dx = (x - cx) / (size / 2.0)
    dy = (y - cy) / (size / 2.0)
    r = np.sqrt(dx*dx + dy*dy)

    # Dispersed smoke clumps
    lobes = [
        (-0.25, -0.22, 0.32, 0.8),
        ( 0.28, -0.18, 0.34, 0.85),
        (-0.22,  0.28, 0.30, 0.75),
        ( 0.25,  0.24, 0.32, 0.8),
        ( 0.00,  0.00, 0.35, 0.7),
    ]

    smoke = np.zeros((size, size), dtype=np.float32)
    for (lcx, lcy, lr, lw) in lobes:
        ldx = (dx - lcx) / lr
        ldy = (dy - lcy) / lr
        dist_sq = ldx * ldx + ldy * ldy
        q = np.clip(1.0 - dist_sq, 0.0, 1.0)
        smoke += (q ** 2) * (3.0 - 2.0 * q) * lw

    max_s = smoke.max()
    if max_s > 0:
        smoke /= max_s

    fading_heat = np.exp(-((r / 0.30)**2.0)) * 0.4

    c_smoke_diss = np.array([48, 58, 74], dtype=np.float32) # Ash blue-gray
    c_amber_heat = np.array([255, 130, 30], dtype=np.float32)

    rgb = np.zeros((size, size, 3), dtype=np.float32)
    for c in range(3):
        col = c_smoke_diss[c] * (1.0 - fading_heat) + c_amber_heat[c] * fading_heat
        rgb[:, :, c] = col

    alpha = np.clip(smoke * 0.7 + fading_heat * 0.35, 0.0, 1.0)
    border_fade = np.clip((0.95 - r) / 0.14, 0.0, 1.0)
    alpha = np.clip(alpha * border_fade * 255.0, 0, 255).astype(np.uint8)

    rgba = np.zeros((size, size, 4), dtype=np.uint8)
    rgba[:, :, :3] = np.clip(rgb, 0, 255).astype(np.uint8)
    rgba[:, :, 3] = alpha

    f_img = Image.fromarray(rgba, 'RGBA')
    draw = ImageDraw.Draw(f_img)

    np.random.seed(1234)
    for _ in range(26):
        ang = np.random.rand() * 2 * math.pi
        sr = np.random.uniform(0.12, 0.88)
        sx = cx + sr * (size / 2.0) * math.cos(ang)
        sy = cy + sr * (size / 2.0) * math.sin(ang)
        draw.ellipse([sx - 1.5, sy - 1.5, sx + 1.5, sy + 1.5], fill=(255, 190, 70, 190))

    return f_img

def generate_explosion(output_path, size=512):
    half = size // 2 # 256
    atlas = Image.new('RGBA', (size, size), (0, 0, 0, 0))

    f0 = render_frame_0(half) # Top-Left
    f1 = render_frame_1(half) # Top-Right
    f2 = render_frame_2(half) # Bottom-Left
    f3 = render_frame_3(half) # Bottom-Right

    atlas.paste(f0, (0, 0))
    atlas.paste(f1, (half, 0))
    atlas.paste(f2, (0, half))
    atlas.paste(f3, (half, half))

    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    atlas.save(output_path, 'PNG', optimize=True)
    print(f"Generated {output_path} ({size}x{size})")

if __name__ == "__main__":
    generate_explosion("/home/danielpdiamon/Whisk3D-Android/app/src/main/assets/textures/fx_explosion.png")
