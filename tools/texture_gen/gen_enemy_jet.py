#!/usr/bin/env python3
"""
gen_enemy_jet.py - Generates textures/enemy_jet.png (512x512 RGBA)
Enemy interceptor jet texture atlas matching the Whisk3D app logo:
- Top-Left (0-256, 0-256): Menacing crimson/amber polarized canopy glass with combat HUD
- Top-Right (256-512, 0-256): Dark metallic splinter camo wings with blood-red chevron stripes & raptor insignia
- Bottom-Left (0-256, 256-512): Carbon stealth belly with dark intake louvers & crimson warning stencils
- Bottom-Right (256-512, 256-512): Blackened tungsten nozzle & fierce high-energy plasma afterburner
"""

import os
import math
import numpy as np
from PIL import Image, ImageDraw, ImageFont

def generate_enemy_jet(output_path, size=512):
    half = size // 2 # 256
    img = Image.new('RGBA', (size, size), (20, 24, 32, 255))
    draw = ImageDraw.Draw(img)

    # -------------------------------------------------------------
    # 1. TOP-LEFT: Crimson / Amber Polarized Cockpit Canopy
    # -------------------------------------------------------------
    y_q1, x_q1 = np.mgrid[:half, :half].astype(np.float32)
    dx_c = (x_q1 - half * 0.5) / (half * 0.45)
    dy_c = (y_q1 - half * 0.5) / (half * 0.45)
    r_c2 = dx_c * dx_c + dy_c * dy_c
    bubble = np.clip(1.0 - r_c2 * 0.45, 0.0, 1.0)

    # Menacing crimson-orange gradient
    q1_rgb = np.zeros((half, half, 3), dtype=np.uint8)
    q1_rgb[:, :, 0] = np.clip(120 + 95 * bubble, 0, 255).astype(np.uint8) # High red
    q1_rgb[:, :, 1] = np.clip(18 + 55 * bubble, 0, 255).astype(np.uint8)  # Low green
    q1_rgb[:, :, 2] = np.clip(12 + 25 * bubble, 0, 255).astype(np.uint8)  # Low blue

    q1_img = Image.fromarray(q1_rgb, 'RGB').convert('RGBA')
    draw_q1 = ImageDraw.Draw(q1_img)

    # Sharp diagonal specular reflection slashes
    draw_q1.polygon([(35, 12), (56, 12), (235, 190), (214, 190)], fill=(255, 235, 215, 210))
    draw_q1.polygon([(68, 12), (76, 12), (250, 185), (242, 185)], fill=(255, 180, 160, 150))
    draw_q1.polygon([(18, 55), (30, 55), (175, 202), (163, 202)], fill=(255, 200, 180, 80))

    # Hostile Lock-on Combat Reticle (Menacing Red/Crimson HUD)
    hud_cx, hud_cy = half // 2, half // 2 + 10
    draw_q1.rectangle([hud_cx - 28, hud_cy - 28, hud_cx + 28, hud_cy + 28], outline=(239, 68, 68, 180), width=2)
    draw_q1.polygon([
        (hud_cx, hud_cy - 36), (hud_cx - 8, hud_cy - 20), (hud_cx + 8, hud_cy - 20)
    ], fill=(239, 68, 68, 220))
    draw_q1.line([(hud_cx - 36, hud_cy), (hud_cx - 16, hud_cy)], fill=(239, 68, 68, 200), width=2)
    draw_q1.line([(hud_cx + 16, hud_cy), (hud_cx + 36, hud_cy)], fill=(239, 68, 68, 200), width=2)
    draw_q1.line([(hud_cx, hud_cy + 16), (hud_cx, hud_cy + 36)], fill=(239, 68, 68, 200), width=2)
    draw_q1.ellipse([hud_cx - 3, hud_cy - 3, hud_cx + 3, hud_cy + 3], fill=(255, 255, 255, 240))

    # Dark carbon-composite canopy frame
    draw_q1.rectangle([0, 0, half, 12], fill=(15, 23, 42, 255))
    draw_q1.rectangle([0, half - 14, half, half], fill=(15, 23, 42, 255))
    draw_q1.rectangle([0, 0, 12, half], fill=(15, 23, 42, 255))
    draw_q1.rectangle([half - 12, 0, half, half], fill=(15, 23, 42, 255))
    for rx in range(16, half - 16, 20):
        draw_q1.ellipse([rx - 2, 4, rx + 2, 8], fill=(71, 85, 105, 255))
        draw_q1.ellipse([rx - 2, half - 10, rx + 2, half - 6], fill=(71, 85, 105, 255))

    img.paste(q1_img, (0, 0))

    # -------------------------------------------------------------
    # 2. TOP-RIGHT: Dark Metallic Camo Wings & Crimson Chevron Accents
    # -------------------------------------------------------------
    q2_img = Image.new('RGBA', (half, half), (30, 41, 59, 255)) # Dark gunmetal stealth composite
    draw_q2 = ImageDraw.Draw(q2_img)

    # Splinter camouflage polygon patches
    draw_q2.polygon([(0, 0), (110, 0), (70, 95), (0, 75)], fill=(15, 23, 42, 255))
    draw_q2.polygon([(140, 120), (256, 90), (256, 180), (170, 210)], fill=(15, 23, 42, 255))
    draw_q2.polygon([(0, 160), (95, 140), (60, 256), (0, 256)], fill=(51, 65, 85, 255))

    # Aggressive Blood-Red Chevron Racing / Combat Stripes
    draw_q2.polygon([(55, 0), (135, 0), (256, 160), (256, 256), (215, 256), (25, 0)], fill=(185, 28, 28, 255)) # #b91c1c
    draw_q2.polygon([(70, 0), (120, 0), (256, 180), (256, 240), (60, 0)], fill=(220, 38, 38, 255)) # #dc2626
    # Fiery crimson-orange warning edge
    draw_q2.polygon([(48, 0), (56, 0), (256, 252), (256, 256), (44, 0)], fill=(249, 115, 22, 255)) # #f97316
    draw_q2.polygon([(134, 0), (142, 0), (256, 150), (256, 160), (130, 0)], fill=(249, 115, 22, 255))

    # Stealth RAM panel lines
    draw_q2.line([(0, 64), (half, 64)], fill=(71, 85, 105, 180), width=2)
    draw_q2.line([(0, 138), (half, 138)], fill=(71, 85, 105, 180), width=2)
    draw_q2.line([(0, 204), (half, 204)], fill=(71, 85, 105, 180), width=2)
    draw_q2.line([(96, 0), (96, half)], fill=(71, 85, 105, 180), width=2)
    draw_q2.line([(180, 0), (180, half)], fill=(71, 85, 105, 180), width=2)

    # Rivet lines
    for py in [64, 138, 204]:
        for px in range(12, half - 12, 16):
            draw_q2.ellipse([px - 1, py - 1, px + 1, py + 1], fill=(15, 23, 42, 220))

    # Enemy Raptor Dagger Insignia (Red Delta Blade)
    insig_cx, insig_cy = 54, 175
    draw_q2.ellipse([insig_cx - 34, insig_cy - 34, insig_cx + 34, insig_cy + 34], fill=(15, 23, 42, 255), outline=(220, 38, 38, 255), width=3)
    # Swept dagger chevron
    draw_q2.polygon([
        (insig_cx, insig_cy - 24),
        (insig_cx + 22, insig_cy + 18),
        (insig_cx, insig_cy + 8),
        (insig_cx - 22, insig_cy + 18)
    ], fill=(239, 68, 68, 255))

    # Vertical Fin Inset Badge: Dark slate with crimson double-chevron & tactical marking
    badge_x0, badge_y0 = 135, 18
    badge_w, badge_h = 105, 105
    draw_q2.polygon([
        (badge_x0 + 10, badge_y0),
        (badge_x0 + badge_w, badge_y0),
        (badge_x0 + badge_w, badge_y0 + badge_h),
        (badge_x0, badge_y0 + badge_h),
    ], fill=(15, 23, 42, 255), outline=(220, 38, 38, 255), width=3)

    # Aggressive Red Delta Chevron
    bx_m = badge_x0 + badge_w // 2
    draw_q2.polygon([
        (bx_m, badge_y0 + 15),
        (badge_x0 + badge_w - 18, badge_y0 + 68),
        (bx_m, badge_y0 + 52),
        (badge_x0 + 18, badge_y0 + 68)
    ], fill=(220, 38, 38, 255))

    try:
        font = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 14)
        draw_q2.text((badge_x0 + 26, badge_y0 + 82), "INTER", fill=(239, 68, 68, 255), font=font)
    except Exception:
        pass

    img.paste(q2_img, (half, 0))

    # -------------------------------------------------------------
    # 3. BOTTOM-LEFT: Carbon Stealth Underside & Weapon Bays
    # -------------------------------------------------------------
    q3_img = Image.new('RGBA', (half, half), (24, 24, 27, 255)) # Deep carbon black
    draw_q3 = ImageDraw.Draw(q3_img)

    draw_q3.rectangle([15, 15, half - 15, half - 15], fill=(39, 39, 42, 255), outline=(63, 63, 70, 255), width=2)

    # Jet intake grill louvers with crimson intake edge
    draw_q3.rectangle([32, 38, half - 32, 44], fill=(220, 38, 38, 255)) # Red intake warning lip
    for louver_y in range(48, half - 45, 20):
        draw_q3.rectangle([35, louver_y, half - 35, louver_y + 11], fill=(9, 9, 11, 255))
        draw_q3.line([(35, louver_y + 2), (half - 35, louver_y + 2)], fill=(82, 82, 91, 255), width=2)

    # Internal bay seams
    draw_q3.rectangle([25, 25, 110, half - 25], outline=(9, 9, 11, 255), width=2)
    draw_q3.rectangle([half - 110, 25, half - 25, half - 25], outline=(9, 9, 11, 255), width=2)

    try:
        font_sm = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 10)
        draw_q3.text((36, 24), "DANGER INTAKE", fill=(239, 68, 68, 230), font=font_sm)
        draw_q3.text((half - 105, 24), "MISSILE BAY", fill=(245, 158, 11, 230), font=font_sm)
    except Exception:
        pass

    img.paste(q3_img, (0, half))

    # -------------------------------------------------------------
    # 4. BOTTOM-RIGHT: Blackened Nozzle & Plasma Exhaust
    # -------------------------------------------------------------
    q4_img = Image.new('RGBA', (half, half), (15, 23, 42, 255))
    draw_q4 = ImageDraw.Draw(q4_img)

    cx_q4, cy_q4 = half // 2, half // 2

    # Blackened tungsten nozzle rim
    draw_q4.ellipse([cx_q4 - 112, cy_q4 - 112, cx_q4 + 112, cy_q4 + 112], fill=(39, 39, 42, 255), outline=(9, 9, 11, 255), width=3)
    for ang_deg in range(0, 360, 20):
        rad = math.radians(ang_deg)
        p1 = (cx_q4 + 88 * math.cos(rad), cy_q4 + 88 * math.sin(rad))
        p2 = (cx_q4 + 112 * math.cos(rad), cy_q4 + 112 * math.sin(rad))
        draw_q4.line([p1, p2], fill=(24, 24, 27, 255), width=2)

    draw_q4.ellipse([cx_q4 - 88, cy_q4 - 88, cx_q4 + 88, cy_q4 + 88], fill=(63, 63, 70, 255), outline=(9, 9, 11, 255), width=3)

    # Violet / Magenta-Orange Plasma Exhaust Plume
    y_af, x_af = np.mgrid[:half, :half].astype(np.float32)
    r_af = np.sqrt((x_af - cx_q4)**2 + (y_af - cy_q4)**2)

    flame_mask = np.clip((82.0 - r_af) / 82.0, 0.0, 1.0)
    
    # Plasma color: White core -> Golden yellow -> Magenta/Violet rim
    af_r = np.clip(255.0 * flame_mask * 1.6, 0, 255).astype(np.uint8)
    af_g = np.clip(180.0 * (flame_mask ** 2.2) * 1.2, 0, 255).astype(np.uint8)
    af_b = np.clip(255.0 * np.clip((flame_mask - 0.2) / 0.8, 0.0, 1.0) ** 1.3, 0, 255).astype(np.uint8)

    af_rgba = np.array(q4_img)
    flame_active = r_af <= 84.0
    af_rgba[flame_active, 0] = af_r[flame_active]
    af_rgba[flame_active, 1] = af_g[flame_active]
    af_rgba[flame_active, 2] = af_b[flame_active]

    q4_img = Image.fromarray(af_rgba, 'RGBA')
    draw_q4 = ImageDraw.Draw(q4_img)

    for d_rad in [14, 28, 44]:
        draw_q4.polygon([
            (cx_q4, cy_q4 - d_rad),
            (cx_q4 + d_rad, cy_q4),
            (cx_q4, cy_q4 + d_rad),
            (cx_q4 - d_rad, cy_q4)
        ], outline=(255, 230, 255, 230), width=2)

    img.paste(q4_img, (half, half))

    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    img.save(output_path, 'PNG', optimize=True)
    print(f"Generated {output_path} ({size}x{size})")

if __name__ == "__main__":
    generate_enemy_jet("/home/danielpdiamon/Whisk3D-Android/app/src/main/assets/textures/enemy_jet.png")
