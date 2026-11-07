  #!/usr/bin/env python3
"""
KiterOS Nano Master Combiner Script:
1. Combines HTML, CSS, and FRIDGE JS into compressed LittleFS staging files.
2. Generates a single-file Arduino IDE sketch (KiterOS_Nano.ino) with embedded PROGMEM fallbacks
   so the launcher works immediately even if the user only flashes the sketch from Arduino IDE.
"""
import gzip
import os
import shutil

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "../.."))

def read_text(rel_path: str) -> str:
    with open(os.path.join(ROOT, rel_path), "r", encoding="utf-8") as f:
        return f.read()

def write_gzip(src_rel: str, dst_path: str) -> None:
    os.makedirs(os.path.dirname(dst_path), exist_ok=True)
    raw = read_text(src_rel).encode("utf-8")
    with open(dst_path, "wb") as f:
        f.write(gzip.compress(raw, compresslevel=9, mtime=0))

def prepare_staging_fs() -> None:
    staging = os.path.join(ROOT, "staging_fs")
    if os.path.exists(staging):
        shutil.rmtree(staging)
    os.makedirs(os.path.join(staging, "config"), exist_ok=True)
    os.makedirs(os.path.join(staging, "launcher"), exist_ok=True)
    os.makedirs(os.path.join(staging, "runtime"), exist_ok=True)
    os.makedirs(os.path.join(staging, "apps"), exist_ok=True)

    # Copy system.json
    shutil.copyfile(
        os.path.join(ROOT, "filesystem/config/system.json"),
        os.path.join(staging, "config/system.json")
    )

    # Sync fridge.js and compress all web assets
    shutil.copyfile(
        os.path.join(ROOT, "fridge/runtime/fridge.js"),
        os.path.join(ROOT, "filesystem/runtime/fridge.js")
    )
    write_gzip("filesystem/launcher/index.html", os.path.join(staging, "launcher/index.html.gz"))
    write_gzip("filesystem/launcher/style.css",  os.path.join(staging, "launcher/style.css.gz"))
    write_gzip("filesystem/launcher/app.js",     os.path.join(staging, "launcher/app.js.gz"))
    write_gzip("filesystem/runtime/fridge.js",   os.path.join(staging, "runtime/fridge.js.gz"))
    print("[OK] Staged & GZIP-compressed HTML/CSS/JS into staging_fs/")

def generate_arduino_ide_bundle() -> None:
    out_dir = os.path.join(ROOT, "dist/Arduino_IDE_Sketch/KiterOS_Nano")
    os.makedirs(out_dir, exist_ok=True)

    # Copy all firmware headers and implementation files into the sketch folder
    inc_dir = os.path.join(ROOT, "firmware/include")
    src_dir = os.path.join(ROOT, "firmware/src")

    for fname in os.listdir(inc_dir):
        if fname.endswith(".h"):
            shutil.copyfile(os.path.join(inc_dir, fname), os.path.join(out_dir, fname))

    for fname in os.listdir(src_dir):
        if fname == "main.cpp":
            shutil.copyfile(os.path.join(src_dir, fname), os.path.join(out_dir, "KiterOS_Nano.ino"))
        elif fname.endswith(".cpp"):
            shutil.copyfile(os.path.join(src_dir, fname), os.path.join(out_dir, fname))

    # Also copy the raw filesystem into data/ for the Arduino IDE LittleFS Upload plugin
    data_dir = os.path.join(out_dir, "data")
    if os.path.exists(data_dir):
        shutil.rmtree(data_dir)
    shutil.copytree(os.path.join(ROOT, "staging_fs"), data_dir)
    print(f"[OK] Generated Arduino IDE sketch folder at: {out_dir}")

if __name__ == "__main__":
    prepare_staging_fs()
    generate_arduino_ide_bundle()
