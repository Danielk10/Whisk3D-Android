#!/usr/bin/env python3
"""
gen_airplane.py - Generates textures/airplane.png (512x512 RGBA)
Airplane texture atlas matching the Whisk3D app logo:
- Top-Left (0-256, 0-256): Cockpit canopy bubble glass with specular reflections & frame
- Top-Right (256-512, 0-256): Upper wings, fuselage & twin fins with 'W'/'WHISK' logo & orange-red stripes
- Bottom-Left (0-256, 256-512): Lower fuselage & wings with intake louvers & mechanical panels
- Bottom-Right (256-512, 256-512): Engine exhaust nozzle & fiery incandescent afterburner flame
"""

import os
import math
import numpy as np
from PIL import Image, ImageDraw, ImageFont

def draw_star(draw, cx, cy, r_outer, r_inner, fill, outline=None):
    pts = []
    for i in range(10):
        r = r_outer if (i % 2 == 0) else r_inner
        ang = -math.pi / 2.0 + i * (math.pi / 5.0)
        pts.append((cx + r * math.cos(ang), cy + r * math.sin(ang)))
    draw.polygon(pts, fill=fill, outline=outline)

def generate_airplane(output_path, size=512):
    half = size // 2 # 256
    img = Image.new('RGBA', (size, size), (40, 48, 60, 255))
    draw = ImageDraw.Draw(img)

    # -------------------------------------------------------------
    # 1. TOP-LEFT QUADRANT (X: 0..256, Y: 0..256): Cockpit Canopy Glass
    # -------------------------------------------------------------
    # Base deep aerospace blue/cyan gradient
    y_q1, x_q1 = np.mgrid[:half, :half].astype(np.float32)
    # Radial curve simulating curved glass bubble
    dx_c = (x_q1 - half * 0.5) / (half * 0.45)
    dy_c = (y_q1 - half * 0.5) / (half * 0.45)
    r_c2 = dx_c * dx_c + dy_c * dy_c
    bubble = np.clip(1.0 - r_c2 * 0.45, 0.0, 1.0)

    # Glass gradient: deep navy/cyan to vibrant azure
    # Center: #0284c7 (2, 132, 199) -> Edge: #034674 (3, 70, 116)
    q1_rgb = np.zeros((half, half, 3), dtype=np.uint8)
    q1_rgb[:, :, 0] = np.clip(8 + 35 * bubble, 0, 255).astype(np.uint8)
    q1_rgb[:, :, 1] = np.clip(70 + 85 * bubble, 0, 255).astype(np.uint8)
    q1_rgb[:, :, 2] = np.clip(140 + 85 * bubble, 0, 255).astype(np.uint8)

    q1_img = Image.fromarray(q1_rgb, 'RGB').convert('RGBA')
    draw_q1 = ImageDraw.Draw(q1_img)

    # Diagonal High-Gloss Specular Highlights across canopy (slashing from top-left to bottom-right)
    # Primary bold specular glare
    draw_q1.polygon([(35, 12), (58, 12), (235, 188), (212, 188)], fill=(255, 255, 255, 220))
    # Secondary thin reflection line
    draw_q1.polygon([(68, 12), (78, 12), (250, 184), (240, 184)], fill=(200, 240, 255, 160))
    # Third soft lower sheen
    draw_q1.polygon([(18, 55), (32, 55), (175, 202), (161, 202)], fill=(255, 255, 255, 90))

    # Tactical HUD display reticle in center of canopy glass
    hud_cx, hud_cy = half // 2, half // 2 + 10
    draw_q1.ellipse([hud_cx - 28, hud_cy - 28, hud_cx + 28, hud_cy + 28], outline=(34, 197, 94, 140), width=2)
    draw_q1.line([(hud_cx - 36, hud_cy), (hud_cx - 16, hud_cy)], fill=(34, 197, 94, 180), width=2)
    draw_q1.line([(hud_cx + 16, hud_cy), (hud_cx + 36, hud_cy)], fill=(34, 197, 94, 180), width=2)
    draw_q1.line([(hud_cx, hud_cy - 36), (hud_cx, hud_cy - 16)], fill=(34, 197, 94, 180), width=2)
    draw_q1.line([(hud_cx, hud_cy + 16), (hud_cx, hud_cy + 36)], fill=(34, 197, 94, 180), width=2)
    draw_q1.rectangle([hud_cx - 2, hud_cy - 2, hud_cx + 2, hud_cy + 2], fill=(34, 197, 94, 230))

    # Titanium canopy frame borders
    draw_q1.rectangle([0, 0, half, 12], fill=(30, 41, 59, 255))
    draw_q1.rectangle([0, half - 14, half, half], fill=(30, 41, 59, 255))
    draw_q1.rectangle([0, 0, 12, half], fill=(30, 41, 59, 255))
    draw_q1.rectangle([half - 12, 0, half, half], fill=(30, 41, 59, 255))
    # Frame rivets
    for rx in range(16, half - 16, 20):
        draw_q1.ellipse([rx - 2, 4, rx + 2, 8], fill=(148, 163, 184, 255))
        draw_q1.ellipse([rx - 2, half - 10, rx + 2, half - 6], fill=(148, 163, 184, 255))

    img.paste(q1_img, (0, 0))

    # -------------------------------------------------------------
    # 2. TOP-RIGHT QUADRANT (X: 256..512, Y: 0..256): Upper Wings & Vertical Fins ('W')
    # -------------------------------------------------------------
    q2_img = Image.new('RGBA', (half, half), (241, 245, 249, 255)) # Clean white/slate-gray composite
    draw_q2 = ImageDraw.Draw(q2_img)

    # Panel shading gradient
    y_q2, x_q2 = np.mgrid[:half, :half].astype(np.float32)
    panel_shade = np.clip(1.0 - 0.08 * (y_q2 / float(half)) - 0.05 * (x_q2 / float(half)), 0.85, 1.0)
    q2_arr = np.array(q2_img)
    for c in range(3):
        q2_arr[:, :, c] = np.clip(q2_arr[:, :, c].astype(np.float32) * panel_shade, 0, 255).astype(np.uint8)
    q2_img = Image.fromarray(q2_arr, 'RGBA')
    draw_q2 = ImageDraw.Draw(q2_img)

    # Dynamic Orange-Red Racing Stripes matching app logo
    # Bold primary diagonal stripe
    draw_q2.polygon([(65, 0), (145, 0), (256, 165), (256, 256), (220, 256), (30, 0)], fill=(230, 57, 70, 255)) # #e63946
    # Inner vibrant orange core
    draw_q2.polygon([(80, 0), (130, 0), (256, 185), (256, 245), (70, 0)], fill=(255, 94, 26, 255)) # #ff5e1a
    # Golden-yellow pinstripe trim
    draw_q2.polygon([(58, 0), (66, 0), (256, 252), (256, 256), (54, 0)], fill=(255, 183, 3, 255)) # #ffb703
    draw_q2.polygon([(144, 0), (152, 0), (256, 155), (256, 166), (140, 0)], fill=(255, 183, 3, 255))

    # Structural panel lines (subtle dark slate)
    draw_q2.line([(0, 64), (half, 64)], fill=(148, 163, 184, 180), width=2)
    draw_q2.line([(0, 138), (half, 138)], fill=(148, 163, 184, 180), width=2)
    draw_q2.line([(0, 204), (half, 204)], fill=(148, 163, 184, 180), width=2)
    draw_q2.line([(96, 0), (96, half)], fill=(148, 163, 184, 180), width=2)
    draw_q2.line([(180, 0), (180, half)], fill=(148, 163, 184, 180), width=2)

    # Rivet lines along panels
    for py in [64, 138, 204]:
        for px in range(12, half - 12, 16):
            draw_q2.ellipse([px - 1, py - 1, px + 1, py + 1], fill=(100, 116, 139, 200))

    # Military Roundel Insignia (Navy Blue circle + White Star + Red/White bars)
    # Positioned at upper-left of quadrant
    r_cx, r_cy, r_rad = 54, 175, 34
    draw_q2.rectangle([r_cx - 48, r_cy - 9, r_cx + 48, r_cy + 9], fill=(220, 38, 38, 255), outline=(255, 255, 255, 255), width=2)
    draw_q2.rectangle([r_cx - 48, r_cy - 4, r_cx + 48, r_cy + 4], fill=(255, 255, 255, 255))
    draw_q2.ellipse([r_cx - r_rad, r_cy - r_rad, r_cx + r_rad, r_cy + r_rad], fill=(30, 58, 138, 255), outline=(255, 255, 255, 255), width=3)
    draw_star(draw_q2, r_cx, r_cy, 22, 9, fill=(255, 255, 255, 255))

    # Vertical Fin Inset Badge: Deep Navy Blue chevron with bold white 'W' & 'WHISK'
    # Positioned in upper right area
    badge_x0, badge_y0 = 135, 18
    badge_w, badge_h = 105, 105
    draw_q2.polygon([
        (badge_x0 + 10, badge_y0),
        (badge_x0 + badge_w, badge_y0),
        (badge_x0 + badge_w, badge_y0 + badge_h),
        (badge_x0, badge_y0 + badge_h),
    ], fill=(30, 64, 175, 255), outline=(255, 255, 255, 255), width=3)

    # Bold White "W"
    # Using polygon geometry for crisp vector typography
    wx0, wy0 = badge_x0 + 20, badge_y0 + 18
    w_pts = [
        (wx0 + 2, wy0), (wx0 + 18, wy0),
        (wx0 + 28, wy0 + 44),
        (wx0 + 38, wy0), (wx0 + 52, wy0),
        (wx0 + 62, wy0 + 44),
        (wx0 + 72, wy0), (wx0 + 88, wy0),
        (wx0 + 72, wy0 + 58), (wx0 + 54, wy0 + 58),
        (wx0 + 45, wy0 + 22),
        (wx0 + 36, wy0 + 58), (wx0 + 18, wy0 + 58),
    ]
    draw_q2.polygon(w_pts, fill=(255, 255, 255, 255))

    # Crisp "WHISK" tactical lettering below "W"
    try:
        font = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 15)
        draw_q2.text((badge_x0 + 22, badge_y0 + 82), "WHISK", fill=(255, 255, 255, 255), font=font)
    except Exception:
        draw_q2.rectangle([badge_x0 + 15, badge_y0 + 82, badge_x0 + badge_w - 10, badge_y0 + 96], fill=(255, 255, 255, 255))

    img.paste(q2_img, (half, 0))

    # -------------------------------------------------------------
    # 3. BOTTOM-LEFT QUADRANT (X: 0..256, Y: 256..512): Lower Fuselage & Wings
    # -------------------------------------------------------------
    q3_img = Image.new('RGBA', (half, half), (71, 85, 105, 255)) # Dark slate underbelly
    draw_q3 = ImageDraw.Draw(q3_img)

    # Panel variation
    draw_q3.rectangle([15, 15, half - 15, half - 15], fill=(51, 65, 85, 255), outline=(100, 116, 139, 255), width=2)

    # Twin Jet Engine Air Intake Grill Louvers
    for louver_y in range(45, half - 45, 20):
        # Deep recessed black vent slot
        draw_q3.rectangle([35, louver_y, half - 35, louver_y + 11], fill=(15, 23, 42, 255))
        # Titanium louver highlight blade
        draw_q3.line([(35, louver_y + 2), (half - 35, louver_y + 2)], fill=(148, 163, 184, 255), width=2)

    # Weapon Bay & Landing Gear Seams
    draw_q3.rectangle([25, 25, 110, half - 25], outline=(15, 23, 42, 255), width=2)
    draw_q3.rectangle([half - 110, 25, half - 25, half - 25], outline=(15, 23, 42, 255), width=2)

    # Stencil Markings
    try:
        font_sm = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 10)
        draw_q3.text((40, 24), "NO STEP", fill=(245, 158, 11, 230), font=font_sm)
        draw_q3.text((half - 100, 24), "ARMAMENT", fill=(239, 68, 68, 230), font=font_sm)
    except Exception:
        pass

    img.paste(q3_img, (0, half))

    # -------------------------------------------------------------
    # 4. BOTTOM-RIGHT QUADRANT (X: 256..512, Y: 256..512): Exhaust Nozzle & Afterburner Flame
    # -------------------------------------------------------------
    q4_img = Image.new('RGBA', (half, half), (30, 41, 59, 255)) # Dark carbon titanium background
    draw_q4 = ImageDraw.Draw(q4_img)

    cx_q4, cy_q4 = half // 2, half // 2

    # Titanium Engine Nozzle Rings
    # Outer dark titanium rim
    draw_q4.ellipse([cx_q4 - 112, cy_q4 - 112, cx_q4 + 112, cy_q4 + 112], fill=(51, 65, 85, 255), outline=(15, 23, 42, 255), width=3)
    # Nozzle iris petals (radial segments)
    for ang_deg in range(0, 360, 20):
        rad = math.radians(ang_deg)
        p1 = (cx_q4 + 88 * math.cos(rad), cy_q4 + 88 * math.sin(rad))
        p2 = (cx_q4 + 112 * math.cos(rad), cy_q4 + 112 * math.sin(rad))
        draw_q4.line([p1, p2], fill=(30, 41, 59, 255), width=2)

    # Mid ring (heat-treated ceramic)
    draw_q4.ellipse([cx_q4 - 88, cy_q4 - 88, cx_q4 + 88, cy_q4 + 88], fill=(71, 85, 105, 255), outline=(15, 23, 42, 255), width=3)

    # Afterburner Flame Plume (Radial concentric intense fire glow)
    y_af, x_af = np.mgrid[:half, :half].astype(np.float32)
    r_af = np.sqrt((x_af - cx_q4)**2 + (y_af - cy_q4)**2)

    # Flame cores:
    # Outer fiery crimson: r <= 80
    # Intense orange: r <= 60
    # Brilliant golden yellow: r <= 42
    # Incandescent white core: r <= 22
    flame_mask = np.clip((82.0 - r_af) / 82.0, 0.0, 1.0)
    
    af_r = np.clip(255.0 * flame_mask * 1.5, 0, 255).astype(np.uint8)
    af_g = np.clip(255.0 * (flame_mask ** 1.8) * 1.3, 0, 255).astype(np.uint8)
    af_b = np.clip(255.0 * np.clip((24.0 - r_af) / 24.0, 0.0, 1.0) ** 1.5, 0, 255).astype(np.uint8)

    af_rgba = np.array(q4_img)
    flame_active = r_af <= 84.0
    af_rgba[flame_active, 0] = af_r[flame_active]
    af_rgba[flame_active, 1] = af_g[flame_active]
    af_rgba[flame_active, 2] = af_b[flame_active]

    q4_img = Image.fromarray(af_rgba, 'RGBA')
    draw_q4 = ImageDraw.Draw(q4_img)

    # Mach Diamond Shock Rings (radiating diamond shock wave rings in flame center)
    for d_rad in [14, 28, 44]:
        draw_q4.polygon([
            (cx_q4, cy_q4 - d_rad),
            (cx_q4 + d_rad, cy_q4),
            (cx_q4, cy_q4 + d_rad),
            (cx_q4 - d_rad, cy_q4)
        ], outline=(255, 255, 240, 220), width=2)

    img.paste(q4_img, (half, half))

    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    img.save(output_path, 'PNG', optimize=True)
    print(f"Generated {output_path} ({size}x{size})")

if __name__ == "__main__":
    generate_airplane("/home/danielpdiamon/Whisk3D-Android/app/src/main/assets/textures/airplane.png")
