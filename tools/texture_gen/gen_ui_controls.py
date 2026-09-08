#!/usr/bin/env python3
"""
gen_ui_controls.py - Generates high-contrast, tactile arcade UI control textures:
- textures/btn_fire.png (256x256)
- textures/btn_missile.png (256x256)
- textures/btn_stick.png (256x256)
- textures/btn_stick_base.png (256x256)
- textures/btn_stick_knob.png (256x256)
- textures/btn_pause.png (128x128)
- textures/btn_sound.png (128x128)
"""

import os
import math
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

def draw_beveled_circle(draw, cx, cy, radius, fill_color, border_color, glow_color, border_w=5):
    # Outer soft glow ring
    for g_rad, g_alpha in [(radius + 8, 40), (radius + 5, 80), (radius + 2, 140)]:
        draw.ellipse([cx - g_rad, cy - g_rad, cx + g_rad, cy + g_rad], outline=(*glow_color, g_alpha), width=2)
    # Outer metallic rim
    draw.ellipse([cx - radius, cy - radius, cx + radius, cy + radius], fill=fill_color, outline=(*border_color, 255), width=border_w)
    # Inner bevel shadow / highlight
    draw.arc([cx - radius + border_w, cy - radius + border_w, cx + radius - border_w, cy + radius - border_w], start=135, end=315, fill=(255, 255, 255, 160), width=2)
    draw.arc([cx - radius + border_w, cy - radius + border_w, cx + radius - border_w, cy + radius - border_w], start=315, end=135, fill=(15, 23, 42, 220), width=3)

def generate_btn_fire(output_path, size=256):
    img = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    cx, cy = size // 2, size // 2
    r_btn = size // 2 - 16

    # Fiery Crimson/Orange Palette
    glow_c = (255, 69, 0)      # #ff4500
    border_c = (239, 68, 68)   # #ef4444
    fill_c = (30, 41, 59)      # Dark slate #1e293b

    draw_beveled_circle(draw, cx, cy, r_btn, fill_c, border_c, glow_c, border_w=7)

    # Recessed button interior pad
    r_pad = r_btn - 12
    draw.ellipse([cx - r_pad, cy - r_pad, cx + r_pad, cy + r_pad], fill=(15, 23, 42, 255), outline=(220, 38, 38, 200), width=3)

    # Crosshair tactical lines
    draw.line([(cx - r_pad + 12, cy), (cx - 36, cy)], fill=(245, 158, 11, 180), width=2)
    draw.line([(cx + 36, cy), (cx + r_pad - 12, cy)], fill=(245, 158, 11, 180), width=2)
    draw.line([(cx, cy - r_pad + 12), (cx, cy - 36)], fill=(245, 158, 11, 180), width=2)
    draw.line([(cx, cy + 36), (cx, cy + r_pad - 12)], fill=(245, 158, 11, 180), width=2)

    # Twin Vulcan Cannon Shells (Golden Cartridges with tracer muzzle flares)
    shell_w, shell_h = 20, 72
    for offset_x in [-22, 22]:
        sx = cx + offset_x
        sy = cy
        # Tracer muzzle flash at tip
        draw.polygon([
            (sx, sy - shell_h//2 - 22),
            (sx - 8, sy - shell_h//2 - 2),
            (sx + 8, sy - shell_h//2 - 2)
        ], fill=(255, 255, 150, 240))
        # Bullet tip (pointed ogive)
        draw.polygon([
            (sx - shell_w//2, sy - shell_h//2 + 18),
            (sx, sy - shell_h//2 - 6),
            (sx + shell_w//2, sy - shell_h//2 + 18)
        ], fill=(255, 215, 0, 255), outline=(255, 255, 255, 255), width=1)
        # Brass cartridge body
        draw.rectangle([sx - shell_w//2, sy - shell_h//2 + 18, sx + shell_w//2, sy + shell_h//2], fill=(245, 158, 11, 255), outline=(180, 83, 9, 255), width=2)
        # Specular glint along cartridge length
        draw.line([(sx - 3, sy - shell_h//2 + 20), (sx - 3, sy + shell_h//2 - 2)], fill=(255, 255, 220, 240), width=2)
        # Cartridge extractor groove & rim
        draw.rectangle([sx - shell_w//2 - 2, sy + shell_h//2 - 6, sx + shell_w//2 + 2, sy + shell_h//2], fill=(217, 119, 6, 255))

    # Center target reticle pip
    draw.ellipse([cx - 5, cy - 5, cx + 5, cy + 5], fill=(255, 255, 255, 255), outline=(239, 68, 68, 255), width=2)

    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    img.save(output_path, 'PNG', optimize=True)
    print(f"Generated {output_path} ({size}x{size})")

def generate_btn_missile(output_path, size=256):
    img = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    cx, cy = size // 2, size // 2
    r_btn = size // 2 - 16

    # Emerald Green / Cyan Neon Palette
    glow_c = (0, 245, 212)     # #00f5d4
    border_c = (16, 185, 129)  # #10b981
    fill_c = (30, 41, 59)      # #1e293b

    draw_beveled_circle(draw, cx, cy, r_btn, fill_c, border_c, glow_c, border_w=7)

    # Recessed pad
    r_pad = r_btn - 12
    draw.ellipse([cx - r_pad, cy - r_pad, cx + r_pad, cy + r_pad], fill=(15, 23, 42, 255), outline=(16, 185, 129, 200), width=3)

    # Tactical Lock-On Reticle (Rotated Square / Diamond with tick marks)
    dia_r = 48
    draw.polygon([
        (cx, cy - dia_r), (cx + dia_r, cy),
        (cx, cy + dia_r), (cx - dia_r, cy)
    ], outline=(16, 185, 129, 140), width=2)

    # Aerodynamic Air-to-Air Missile Icon
    # Center-aligned vertically
    msl_w = 26
    msl_len = 110
    m_top_y = cy - msl_len // 2 + 10
    m_bot_y = cy + msl_len // 2 - 10

    # Rocket thruster flame
    draw.polygon([
        (cx, m_bot_y + 36),
        (cx - 10, m_bot_y + 4),
        (cx + 10, m_bot_y + 4)
    ], fill=(255, 107, 53, 240))
    draw.polygon([
        (cx, m_bot_y + 22),
        (cx - 5, m_bot_y + 4),
        (cx + 5, m_bot_y + 4)
    ], fill=(255, 230, 100, 255))

    # Crimson Delta Tail Fins
    draw.polygon([
        (cx - msl_w//2, m_bot_y - 28),
        (cx - msl_w//2 - 24, m_bot_y + 4),
        (cx - msl_w//2, m_bot_y + 4)
    ], fill=(239, 68, 68, 255), outline=(185, 28, 28, 255), width=1)
    draw.polygon([
        (cx + msl_w//2, m_bot_y - 28),
        (cx + msl_w//2 + 24, m_bot_y + 4),
        (cx + msl_w//2, m_bot_y + 4)
    ], fill=(239, 68, 68, 255), outline=(185, 28, 28, 255), width=1)

    # Mid-body forward canard fins
    draw.polygon([
        (cx - msl_w//2, m_top_y + 32),
        (cx - msl_w//2 - 14, m_top_y + 48),
        (cx - msl_w//2, m_top_y + 48)
    ], fill=(239, 68, 68, 255))
    draw.polygon([
        (cx + msl_w//2, m_top_y + 32),
        (cx + msl_w//2 + 14, m_top_y + 48),
        (cx + msl_w//2, m_top_y + 48)
    ], fill=(239, 68, 68, 255))

    # Missile Fuselage (Clean White Ceramic/Composite)
    draw.rectangle([cx - msl_w//2, m_top_y + 24, cx + msl_w//2, m_bot_y], fill=(248, 250, 252, 255), outline=(148, 163, 184, 255), width=2)
    # Specular shine
    draw.line([(cx - 4, m_top_y + 26), (cx - 4, m_bot_y - 2)], fill=(255, 255, 255, 240), width=2)

    # Radar/Infrared Seeker Nose Cone (Faceted ogive)
    draw.polygon([
        (cx - msl_w//2, m_top_y + 24),
        (cx, m_top_y),
        (cx + msl_w//2, m_top_y + 24)
    ], fill=(245, 158, 11, 255), outline=(255, 255, 255, 255), width=2)
    # Glowing infrared optical sensor dome tip
    draw.ellipse([cx - 4, m_top_y - 2, cx + 4, m_top_y + 6], fill=(255, 255, 255, 255))

    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    img.save(output_path, 'PNG', optimize=True)
    print(f"Generated {output_path} ({size}x{size})")

def generate_btn_stick(output_path, size=256):
    """
    Joystick texture designed to look amazing BOTH as the base (110x110)
    and as the knob (56x56) in Renderer.cpp!
    """
    img = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    cx, cy = size // 2, size // 2
    r_outer = size // 2 - 14

    # Glowing Electric Cyan/Blue Theme
    glow_c = (56, 189, 248)    # #38bdf8
    border_c = (14, 165, 233)  # #0ea5e9
    fill_c = (30, 41, 59)      # #1e293b

    draw_beveled_circle(draw, cx, cy, r_outer, fill_c, border_c, glow_c, border_w=6)

    # Darker gimbal well
    r_well = r_outer - 10
    draw.ellipse([cx - r_well, cy - r_well, cx + r_well, cy + r_well], fill=(15, 23, 42, 255), outline=(56, 189, 248, 160), width=2)

    # 4 Cardinal Directional Arrows (North, South, East, West)
    arrow_dist = r_outer - 22
    arrow_sz = 10
    # North
    draw.polygon([(cx, cy - arrow_dist - arrow_sz), (cx - arrow_sz, cy - arrow_dist), (cx + arrow_sz, cy - arrow_dist)], fill=(56, 189, 248, 220))
    # South
    draw.polygon([(cx, cy + arrow_dist + arrow_sz), (cx - arrow_sz, cy + arrow_dist), (cx + arrow_sz, cy + arrow_dist)], fill=(56, 189, 248, 220))
    # East
    draw.polygon([(cx + arrow_dist + arrow_sz, cy), (cx + arrow_dist, cy - arrow_sz), (cx + arrow_dist, cy + arrow_sz)], fill=(56, 189, 248, 220))
    # West
    draw.polygon([(cx - arrow_dist - arrow_sz, cy), (cx - arrow_dist, cy - arrow_sz), (cx - arrow_dist, cy + arrow_sz)], fill=(56, 189, 248, 220))

    # Concentric Tactical Graduation Ring
    r_mid = 62
    draw.ellipse([cx - r_mid, cy - r_mid, cx + r_mid, cy + r_mid], outline=(56, 189, 248, 180), width=2)
    for ang in range(0, 360, 30):
        rad = math.radians(ang)
        p1 = (cx + (r_mid - 6) * math.cos(rad), cy + (r_mid - 6) * math.sin(rad))
        p2 = (cx + (r_mid + 6) * math.cos(rad), cy + (r_mid + 6) * math.sin(rad))
        draw.line([p1, p2], fill=(56, 189, 248, 220), width=2)

    # Central Thumbpad / Knob Cap (Functions as thumbstick head)
    r_knob = 36
    draw.ellipse([cx - r_knob, cy - r_knob, cx + r_knob, cy + r_knob], fill=(51, 65, 85, 255), outline=(224, 242, 254, 255), width=3)
    # Knurled grip grooves
    draw.ellipse([cx - (r_knob - 8), cy - (r_knob - 8), cx + (r_knob - 8), cy + (r_knob - 8)], outline=(100, 116, 139, 255), width=2)
    # Central glowing tactile pip
    draw.ellipse([cx - 12, cy - 12, cx + 12, cy + 12], fill=(14, 165, 233, 255), outline=(255, 255, 255, 255), width=2)
    draw.ellipse([cx - 4, cy - 4, cx + 4, cy + 4], fill=(255, 255, 255, 255))

    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    img.save(output_path, 'PNG', optimize=True)
    print(f"Generated {output_path} ({size}x{size})")

def generate_btn_stick_base(output_path, size=256):
    """Dedicated joystick base boundary ring"""
    img = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    cx, cy = size // 2, size // 2
    r_outer = size // 2 - 14

    glow_c = (56, 189, 248)
    border_c = (14, 165, 233)
    fill_c = (15, 23, 42)

    draw_beveled_circle(draw, cx, cy, r_outer, fill_c, border_c, glow_c, border_w=6)

    # Concentric tactical circles
    for r_i in [78, 54]:
        draw.ellipse([cx - r_i, cy - r_i, cx + r_i, cy + r_i], outline=(56, 189, 248, 120), width=1)

    # 8-Way Directional Guides
    for ang in range(0, 360, 45):
        rad = math.radians(ang)
        p1 = (cx + 42 * math.cos(rad), cy + 42 * math.sin(rad))
        p2 = (cx + (r_outer - 14) * math.cos(rad), cy + (r_outer - 14) * math.sin(rad))
        draw.line([p1, p2], fill=(56, 189, 248, 100), width=2)

    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    img.save(output_path, 'PNG', optimize=True)
    print(f"Generated {output_path} ({size}x{size})")

def generate_btn_stick_knob(output_path, size=256):
    """Dedicated tactile 3D thumbstick knob"""
    img = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    cx, cy = size // 2, size // 2
    r_knob = size // 2 - 16

    glow_c = (56, 189, 248)
    border_c = (224, 242, 254)
    fill_c = (51, 65, 85)

    draw_beveled_circle(draw, cx, cy, r_knob, fill_c, border_c, glow_c, border_w=6)

    # Anti-slip concentric grip ribs
    for r_g in [r_knob - 18, r_knob - 32]:
        draw.ellipse([cx - r_g, cy - r_g, cx + r_g, cy + r_g], outline=(30, 41, 59, 255), width=3)

    # Center glowing jewel pip
    draw.ellipse([cx - 24, cy - 24, cx + 24, cy + 24], fill=(14, 165, 233, 255), outline=(255, 255, 255, 255), width=3)
    draw.ellipse([cx - 8, cy - 8, cx + 8, cy + 8], fill=(255, 255, 255, 255))

    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    img.save(output_path, 'PNG', optimize=True)
    print(f"Generated {output_path} ({size}x{size})")

def generate_btn_pause(output_path, size=128):
    img = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    cx, cy = size // 2, size // 2
    r_btn = size // 2 - 8

    glow_c = (56, 189, 248)
    border_c = (14, 165, 233)
    fill_c = (30, 41, 59)

    draw_beveled_circle(draw, cx, cy, r_btn, fill_c, border_c, glow_c, border_w=4)

    # Twin Pause Bars (Rounded Neon Cyan Bars)
    bar_w = 12
    bar_h = 44
    gap = 12

    for offset in [-bar_w - gap//2, gap//2]:
        bx = cx + offset
        by = cy - bar_h // 2
        # Bar glow shadow
        draw.rounded_rectangle([bx - 1, by - 1, bx + bar_w + 1, by + bar_h + 1], radius=4, fill=(14, 165, 233, 180))
        # Bar face
        draw.rounded_rectangle([bx, by, bx + bar_w, by + bar_h], radius=3, fill=(224, 242, 254, 255), outline=(56, 189, 248, 255), width=1)

    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    img.save(output_path, 'PNG', optimize=True)
    print(f"Generated {output_path} ({size}x{size})")

def generate_btn_sound(output_path, size=128):
    img = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    cx, cy = size // 2, size // 2
    r_btn = size // 2 - 8

    glow_c = (56, 189, 248)
    border_c = (14, 165, 233)
    fill_c = (30, 41, 59)

    draw_beveled_circle(draw, cx, cy, r_btn, fill_c, border_c, glow_c, border_w=4)

    # Modern Acoustic Speaker Cone + 2 Sound Wave Arcs
    spk_x = cx - 14
    # Back box of speaker
    draw.rectangle([spk_x - 16, cy - 12, spk_x - 6, cy + 12], fill=(224, 242, 254, 255))
    # Flare cone
    draw.polygon([
        (spk_x - 6, cy - 12),
        (spk_x + 12, cy - 24),
        (spk_x + 12, cy + 24),
        (spk_x - 6, cy + 12)
    ], fill=(224, 242, 254, 255), outline=(56, 189, 248, 255), width=1)

    # Sound wave arc 1 (inner)
    draw.arc([spk_x + 4, cy - 18, spk_x + 28, cy + 18], start=-45, end=45, fill=(56, 189, 248, 255), width=3)
    # Sound wave arc 2 (outer)
    draw.arc([spk_x + 12, cy - 28, spk_x + 44, cy + 28], start=-45, end=45, fill=(56, 189, 248, 255), width=3)

    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    img.save(output_path, 'PNG', optimize=True)
    print(f"Generated {output_path} ({size}x{size})")

def generate_all_ui(tex_dir):
    generate_btn_fire(os.path.join(tex_dir, "btn_fire.png"), size=256)
    generate_btn_missile(os.path.join(tex_dir, "btn_missile.png"), size=256)
    generate_btn_stick(os.path.join(tex_dir, "btn_stick.png"), size=256)
    generate_btn_stick_base(os.path.join(tex_dir, "btn_stick_base.png"), size=256)
    generate_btn_stick_knob(os.path.join(tex_dir, "btn_stick_knob.png"), size=256)
    generate_btn_pause(os.path.join(tex_dir, "btn_pause.png"), size=128)
    generate_btn_sound(os.path.join(tex_dir, "btn_sound.png"), size=128)

if __name__ == "__main__":
    tex_dir = "/home/danielpdiamon/Whisk3D-Android/app/src/main/assets/textures"
    generate_all_ui(tex_dir)
