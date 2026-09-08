#!/usr/bin/env python3
"""
gen_target.py - Generates textures/target.png (512x512 RGBA)
Warship / Battleship texture atlas matching the Whisk3D app logo:
- Top-Left (U: 0.10-0.45, Y: 25-205): Forward deck with 16-cell VLS missile silos & main gun turret
- Top-Right (U: 0.55-0.95, Y: 25-205): Aft flight deck with bold 'H' helipad, CIWS turret & safety nets
- Center-Top: Bridge roof with phased array radar dome & exhaust stacks
- Bottom (Y: 281-512): Bridge windows, steel hull plating, rivets, 'DDG-88' pennant & oxide red waterline
"""

import os
import math
import numpy as np
from PIL import Image, ImageDraw, ImageFont

def generate_target(output_path, size=512):
    img = Image.new('RGBA', (size, size), (30, 41, 59, 255)) # Naval slate gray #1e293b
    draw = ImageDraw.Draw(img)

    # -------------------------------------------------------------
    # 1. TOP-LEFT: Forward Deck (VLS Missile Silos + Main Naval Gun)
    # Range: X ~ [51, 230], Y ~ [25, 205]
    # -------------------------------------------------------------
    deck_fx0, deck_fy0 = 50, 20
    deck_fx1, deck_fy1 = 232, 210

    # Non-skid naval deck plate
    draw.rectangle([deck_fx0, deck_fy0, deck_fx1, deck_fy1], fill=(47, 58, 76, 255), outline=(15, 23, 42, 255), width=2)

    # Main Naval Gun Turret (Faceted stealth turret with dual forward barrels)
    # Turret base mount
    turret_cx, turret_cy = 141, 62
    draw.ellipse([turret_cx - 28, turret_cy - 28, turret_cx + 28, turret_cy + 28], fill=(30, 41, 59, 255), outline=(71, 85, 105, 255), width=2)
    # Angled stealth turret housing
    draw.polygon([
        (turret_cx - 20, turret_cy + 18),
        (turret_cx + 20, turret_cy + 18),
        (turret_cx + 14, turret_cy - 16),
        (turret_cx - 14, turret_cy - 16)
    ], fill=(51, 65, 85, 255), outline=(100, 116, 139, 255), width=2)
    # Twin 127mm gun barrels pointing forward (up in PNG)
    draw.rectangle([turret_cx - 7, turret_cy - 38, turret_cx - 3, turret_cy - 16], fill=(15, 23, 42, 255), outline=(148, 163, 184, 255), width=1)
    draw.rectangle([turret_cx + 3, turret_cy - 38, turret_cx + 7, turret_cy - 16], fill=(15, 23, 42, 255), outline=(148, 163, 184, 255), width=1)
    # Barrel muzzle brakes
    draw.rectangle([turret_cx - 8, turret_cy - 41, turret_cx - 2, turret_cy - 38], fill=(203, 213, 225, 255))
    draw.rectangle([turret_cx + 2, turret_cy - 41, turret_cx + 8, turret_cy - 38], fill=(203, 213, 225, 255))

    # 16-Cell Vertical Launch System (VLS) Missile Battery
    # Positioned behind main gun
    vls_x0, vls_y0 = 68, 105
    vls_w, vls_h = 146, 92
    draw.rectangle([vls_x0, vls_y0, vls_x0 + vls_w, vls_y0 + vls_h], fill=(30, 41, 59, 255), outline=(15, 23, 42, 255), width=2)

    # Hazard caution stripes along VLS perimeter
    for hx in range(vls_x0, vls_x0 + vls_w - 6, 12):
        draw.polygon([(hx, vls_y0), (hx + 6, vls_y0), (hx, vls_y0 + 6)], fill=(245, 158, 11, 255))
        draw.polygon([(hx, vls_y0 + vls_h), (hx + 6, vls_y0 + vls_h), (hx + 6, vls_y0 + vls_h - 6)], fill=(245, 158, 11, 255))

    # 4x4 Hatch Cells Grid
    cols, rows = 4, 4
    cell_w = (vls_w - 18) / float(cols)
    cell_h = (vls_h - 18) / float(rows)

    for r in range(rows):
        for c in range(cols):
            cx = vls_x0 + 9 + c * cell_w
            cy = vls_y0 + 9 + r * cell_h
            # Outer cell bezel
            draw.rectangle([cx + 1, cy + 1, cx + cell_w - 2, cy + cell_h - 2], fill=(15, 23, 42, 255), outline=(71, 85, 105, 255), width=1)
            # Inner armored missile hatch door
            draw.rectangle([cx + 3, cy + 3, cx + cell_w - 4, cy + cell_h - 4], fill=(51, 65, 85, 255))
            # Center exhaust relief vent / latch
            draw.ellipse([cx + cell_w/2 - 2, cy + cell_h/2 - 2, cx + cell_w/2 + 2, cy + cell_h/2 + 2], fill=(148, 163, 184, 255))

    # Anchor windlass & chains on forward deck
    draw.line([(55, 32), (72, 54)], fill=(148, 163, 184, 255), width=2)
    draw.line([(227, 32), (210, 54)], fill=(148, 163, 184, 255), width=2)

    # -------------------------------------------------------------
    # 2. TOP-RIGHT: Aft Flight Deck (Helipad with 'H' + CIWS)
    # Range: X ~ [281, 486], Y ~ [25, 205]
    # -------------------------------------------------------------
    heli_x0, heli_y0 = 280, 20
    heli_x1, heli_y1 = 490, 210

    # Flight deck dark green / slate non-skid surface
    draw.rectangle([heli_x0, heli_y0, heli_x1, heli_y1], fill=(40, 52, 60, 255), outline=(15, 23, 42, 255), width=2)

    # Diagonal yellow approach hazard boundary lines
    draw.line([(heli_x0 + 10, heli_y0 + 10), (heli_x0 + 45, heli_y0 + 10)], fill=(245, 158, 11, 255), width=3)
    draw.line([(heli_x0 + 10, heli_y0 + 10), (heli_x0 + 10, heli_y0 + 45)], fill=(245, 158, 11, 255), width=3)
    draw.line([(heli_x1 - 10, heli_y0 + 10), (heli_x1 - 45, heli_y0 + 10)], fill=(245, 158, 11, 255), width=3)
    draw.line([(heli_x1 - 10, heli_y0 + 10), (heli_x1 - 10, heli_y0 + 45)], fill=(245, 158, 11, 255), width=3)

    # Helipad Center Markings
    hcx, hcy = (heli_x0 + heli_x1) // 2, (heli_y0 + heli_y1) // 2
    hr = 68

    # Outer crisp white circle ring
    draw.ellipse([hcx - hr, hcy - hr, hcx + hr, hcy + hr], outline=(255, 255, 255, 255), width=6)
    # Inner dashed ring / tie-downs
    draw.ellipse([hcx - (hr - 14), hcy - (hr - 14), hcx + (hr - 14), hcy + (hr - 14)], outline=(245, 158, 11, 200), width=2)

    # Bold White Letter 'H'
    hw, hh, ht = 28, 52, 10
    # Left vertical leg
    draw.rectangle([hcx - hw - ht, hcy - hh//2, hcx - hw, hcy + hh//2], fill=(255, 255, 255, 255))
    # Right vertical leg
    draw.rectangle([hcx + hw, hcy - hh//2, hcx + hw + ht, hcy + hh//2], fill=(255, 255, 255, 255))
    # Crossbar
    draw.rectangle([hcx - hw, hcy - ht//2, hcx + hw, hcy + ht//2], fill=(255, 255, 255, 255))

    # CIWS Phalanx Anti-Missile Gatling Gun Turret on aft platform
    ciws_cx, ciws_cy = heli_x0 + 32, heli_y0 + 32
    draw.ellipse([ciws_cx - 14, ciws_cy - 14, ciws_cx + 14, ciws_cy + 14], fill=(241, 245, 249, 255), outline=(71, 85, 105, 255), width=2)
    draw.rectangle([ciws_cx - 3, ciws_cy - 24, ciws_cx + 3, ciws_cy - 12], fill=(15, 23, 42, 255))

    # Flight deck perimeter safety lifelines / nets
    for rx in range(heli_x0 + 10, heli_x1 - 10, 16):
        draw.ellipse([rx - 1, heli_y1 - 4, rx + 1, heli_y1 - 1], fill=(203, 213, 225, 255))

    # -------------------------------------------------------------
    # 3. CENTER-TOP: Bridge Roof (U: 0.25-0.75, Y: 76-179)
    # -------------------------------------------------------------
    # SPY Phased-Array Radar Radome (Spherical white dome)
    radar_cx, radar_cy = size // 2, 60
    draw.ellipse([radar_cx - 24, radar_cy - 24, radar_cx + 24, radar_cy + 24], fill=(248, 250, 252, 255), outline=(148, 163, 184, 255), width=2)
    draw.ellipse([radar_cx - 18, radar_cy - 20, radar_cx - 8, radar_cy - 10], fill=(255, 255, 255, 255)) # Specular

    # Turbine exhaust stack with black heat grates
    draw.rectangle([radar_cx - 16, radar_cy + 28, radar_cx + 16, radar_cy + 52], fill=(15, 23, 42, 255), outline=(71, 85, 105, 255), width=2)
    for gy in range(radar_cy + 32, radar_cy + 50, 4):
        draw.line([(radar_cx - 14, gy), (radar_cx + 14, gy)], fill=(100, 116, 139, 255), width=1)

    # -------------------------------------------------------------
    # 4. BOTTOM HALF: Bridge Windows, Hull Plating, DDG-88 & Oxide Red Waterline
    # Range: Y: 250 to 512
    # -------------------------------------------------------------
    hull_y0 = 248

    # Bridge Observation Windows (Y: 275 to 320)
    # Armored panoramic navigation bridge windows with glowing cyan/greenish bridge screens
    draw.rectangle([45, 275, size - 45, 318], fill=(15, 23, 42, 255), outline=(71, 85, 105, 255), width=2)

    win_w = 28
    win_gap = 10
    num_wins = 11
    start_wx = (size - (num_wins * win_w + (num_wins - 1) * win_gap)) // 2

    for w_i in range(num_wins):
        wx = start_wx + w_i * (win_w + win_gap)
        # Window glass: glowing cyber-cyan / emerald navigation displays
        win_color = (14, 165, 233, 255) if (w_i % 2 == 0) else (16, 185, 129, 255) # Alternate cyan/emerald
        draw.rectangle([wx, 280, wx + win_w, 312], fill=win_color, outline=(224, 242, 254, 255), width=1)
        # Windshield specular slash
        draw.line([(wx + 3, 310), (wx + win_w - 4, 282)], fill=(255, 255, 255, 180), width=2)

    # Hull Armor Plating Grid (Y: 330 to 455)
    # Vertical welded plate joints
    for hx in range(40, size - 30, 48):
        draw.line([(hx, 325), (hx, 460)], fill=(15, 23, 42, 255), width=2)
        # Flush rivets along plate joint
        for ry in range(332, 455, 16):
            draw.ellipse([hx - 2, ry - 1, hx + 2, ry + 1], fill=(148, 163, 184, 200))

    # Horizontal armor sheer strakes
    draw.line([(0, 345), (size, 345)], fill=(71, 85, 105, 255), width=2)
    draw.line([(0, 395), (size, 395)], fill=(71, 85, 105, 255), width=2)
    draw.line([(0, 445), (size, 445)], fill=(71, 85, 105, 255), width=2)

    # Warship Pennant Hull Number: "DDG-88" in bold naval block typography
    try:
        font_hull = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 26)
        # Drop shadow
        draw.text((72, 357), "DDG-88", fill=(15, 23, 42, 255), font=font_hull)
        # Bold white numbers
        draw.text((70, 355), "DDG-88", fill=(255, 255, 255, 255), font=font_hull)
        
        # Aegis logo / insignia
        draw.text((size - 170, 355), "AEGIS", fill=(255, 255, 255, 220), font=font_hull)
    except Exception:
        pass

    # Anchor Hawsehole on bow side
    draw.ellipse([45, 360, 62, 385], fill=(15, 23, 42, 255), outline=(100, 116, 139, 255), width=2)

    # Waterline Boot-Topping Stripe (Y: 460 to 512)
    # Rich anti-fouling oxide red matching naval battleships
    draw.rectangle([0, 460, size, 466], fill=(15, 23, 42, 255)) # Black boot-top dividing line
    draw.rectangle([0, 466, size, size], fill=(153, 27, 27, 255)) # Rich oxide red #991b1b
    # Lower keel shading
    draw.rectangle([0, 498, size, size], fill=(127, 29, 29, 255)) # Deep oxide shadow #7f1d1d

    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    img.save(output_path, 'PNG', optimize=True)
    print(f"Generated {output_path} ({size}x{size})")

if __name__ == "__main__":
    generate_target("/home/danielpdiamon/Whisk3D-Android/app/src/main/assets/textures/target.png")
