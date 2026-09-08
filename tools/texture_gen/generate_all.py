#!/usr/bin/env python3
"""
generate_all.py - Master texture generation runner for Whisk3D-Android.
Generates and validates all stylized textures matching ic_launcher-playstore.png:
1. sun.png (512x512)
2. cloud.png (512x512)
3. sea.png (512x512)
4. terrain.png (512x512)
5. airplane.png (512x512)
6. enemy_jet.png (512x512)
7. target.png (512x512)
8. fx_explosion.png (512x512)
9. UI controls: btn_fire, btn_missile, btn_stick, btn_stick_base, btn_stick_knob, btn_pause, btn_sound
"""

import os
import sys
from PIL import Image

# Import individual generators
from gen_sun import generate_sun
from gen_cloud import generate_cloud
from gen_sea import generate_sea
from gen_terrain import generate_terrain
from gen_airplane import generate_airplane
from gen_enemy_jet import generate_enemy_jet
from gen_target import generate_target
from gen_explosion import generate_explosion
from gen_ui_controls import generate_all_ui

def verify_png(path, expected_size=None, expected_mode='RGBA'):
    if not os.path.exists(path):
        return False, f"File does not exist: {path}"
    try:
        with Image.open(path) as img:
            img.verify()
        with Image.open(path) as img:
            size = img.size
            mode = img.mode
            file_size_kb = os.path.getsize(path) / 1024.0
            if expected_size and size != expected_size:
                return False, f"Size mismatch: got {size}, expected {expected_size}"
            if expected_mode and mode != expected_mode:
                return False, f"Mode mismatch: got {mode}, expected {expected_mode}"
            return True, f"OK - {size[0]}x{size[1]} {mode} ({file_size_kb:.1f} KB)"
    except Exception as e:
        return False, f"Invalid image {path}: {e}"

def main():
    repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), "../.."))
    tex_dir = os.path.join(repo_root, "app/src/main/assets/textures")
    os.makedirs(tex_dir, exist_ok=True)

    print("=========================================================")
    print("Whisk3D Texture Generator Suite - Executing All Tasks")
    print(f"Target Assets Directory: {tex_dir}")
    print("=========================================================")

    tasks = [
        ("Sun (512x512)", lambda: generate_sun(os.path.join(tex_dir, "sun.png"), size=512), "sun.png", (512, 512)),
        ("Cloud (512x512)", lambda: generate_cloud(os.path.join(tex_dir, "cloud.png"), size=512), "cloud.png", (512, 512)),
        ("Sea (512x512)", lambda: generate_sea(os.path.join(tex_dir, "sea.png"), size=512), "sea.png", (512, 512)),
        ("Terrain (512x512)", lambda: generate_terrain(os.path.join(tex_dir, "terrain.png"), size=512), "terrain.png", (512, 512)),
        ("Airplane (512x512)", lambda: generate_airplane(os.path.join(tex_dir, "airplane.png"), size=512), "airplane.png", (512, 512)),
        ("Enemy Jet (512x512)", lambda: generate_enemy_jet(os.path.join(tex_dir, "enemy_jet.png"), size=512), "enemy_jet.png", (512, 512)),
        ("Target Warship (512x512)", lambda: generate_target(os.path.join(tex_dir, "target.png"), size=512), "target.png", (512, 512)),
        ("Explosion (512x512)", lambda: generate_explosion(os.path.join(tex_dir, "fx_explosion.png"), size=512), "fx_explosion.png", (512, 512)),
        ("UI Controls", lambda: generate_all_ui(tex_dir), None, None),
    ]

    for name, gen_fn, _, _ in tasks:
        print(f"\n[Generating] {name}...")
        gen_fn()

    print("\n=========================================================")
    print("Verification & Integrity Checks")
    print("=========================================================")

    all_expected = [
        ("textures/sun.png", (512, 512), "RGBA"),
        ("textures/cloud.png", (512, 512), "RGBA"),
        ("textures/sea.png", (512, 512), "RGBA"),
        ("textures/terrain.png", (512, 512), "RGBA"),
        ("textures/airplane.png", (512, 512), "RGBA"),
        ("textures/enemy_jet.png", (512, 512), "RGBA"),
        ("textures/target.png", (512, 512), "RGBA"),
        ("textures/fx_explosion.png", (512, 512), "RGBA"),
        ("textures/btn_fire.png", (256, 256), "RGBA"),
        ("textures/btn_missile.png", (256, 256), "RGBA"),
        ("textures/btn_stick.png", (256, 256), "RGBA"),
        ("textures/btn_stick_base.png", (256, 256), "RGBA"),
        ("textures/btn_stick_knob.png", (256, 256), "RGBA"),
        ("textures/btn_pause.png", (128, 128), "RGBA"),
        ("textures/btn_sound.png", (128, 128), "RGBA"),
    ]

    all_ok = True
    for rel_path, exp_size, exp_mode in all_expected:
        full_path = os.path.join(repo_root, "app/src/main/assets", rel_path)
        ok, msg = verify_png(full_path, exp_size, exp_mode)
        status = "PASSED" if ok else "FAILED"
        print(f"[{status}] {rel_path:28s} -> {msg}")
        if not ok:
            all_ok = False

    print("=========================================================")
    if all_ok:
        print("ALL 15 TEXTURES GENERATED AND VERIFIED SUCCESSFULLY!")
    else:
        print("SOME VERIFICATION CHECKS FAILED.")
        sys.exit(1)

if __name__ == "__main__":
    main()
