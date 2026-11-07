#!/usr/bin/env python3
"""
Master build script for KiterOS Nano:
1. Gzip-compresses HTML, CSS, and JS into C++ PROGMEM byte arrays (embedded_web.h).
2. Prepares LittleFS filesystem staging files.
3. Assembles a standalone Arduino IDE sketch folder containing KiterOS_Nano.ino,
   all module sources (.cpp), headers (.h), and the data/ directory.
"""
import gzip
import os
import shutil

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "../.."))

def read_bytes(rel_path: str) -> bytes:
    with open(os.path.join(ROOT, rel_path), "rb") as f:
        return f.read()

def to_c_array(var_name: str, raw_bytes: bytes) -> str:
    gz = gzip.compress(raw_bytes, compresslevel=9, mtime=0)
    hex_items = ", ".join(f"0x{b:02x}" for b in gz)
    return (
        f"const size_t {var_name}_LEN = {len(gz)};\n"
        f"const uint8_t {var_name}[] PROGMEM = {{ {hex_items} }};\n\n"
    )

def generate_embedded_header() -> None:
    src_fridge = os.path.join(ROOT, "fridge/runtime/fridge.js")
    dst_fridge = os.path.join(ROOT, "filesystem/runtime/fridge.js")
    os.makedirs(os.path.dirname(dst_fridge), exist_ok=True)
    if os.path.exists(src_fridge):
        shutil.copyfile(src_fridge, dst_fridge)

    header_path = os.path.join(ROOT, "firmware/include/embedded_web.h")
    os.makedirs(os.path.dirname(header_path), exist_ok=True)

    with open(header_path, "w", encoding="utf-8") as f:
        f.write("#ifndef KITEROS_EMBEDDED_WEB_H\n#define KITEROS_EMBEDDED_WEB_H\n\n")
        f.write("#include <Arduino.h>\n\n")
        f.write(to_c_array("EMBED_INDEX_HTML_GZ", read_bytes("filesystem/launcher/index.html")))
        f.write(to_c_array("EMBED_STYLE_CSS_GZ",  read_bytes("filesystem/launcher/style.css")))
        f.write(to_c_array("EMBED_APP_JS_GZ",     read_bytes("filesystem/launcher/app.js")))
        f.write(to_c_array("EMBED_FRIDGE_JS_GZ",  read_bytes("filesystem/runtime/fridge.js")))
        f.write("#endif\n")
    print(f"[OK] Generated firmware/include/embedded_web.h")

def prepare_staging_fs() -> None:
    staging = os.path.join(ROOT, "staging_fs")
    if os.path.exists(staging):
        shutil.rmtree(staging)
    os.makedirs(os.path.join(staging, "config"), exist_ok=True)
    os.makedirs(os.path.join(staging, "apps"), exist_ok=True)

    cfg_src = os.path.join(ROOT, "filesystem/config/system.json")
    if os.path.exists(cfg_src):
        shutil.copyfile(cfg_src, os.path.join(staging, "config/system.json"))
    else:
        with open(os.path.join(staging, "config/system.json"), "w") as f:
            f.write('{"ssid":"KiterOS-Nano","password":"kiteros8266","channel":1}')
    print("[OK] Staging filesystem populated")

def generate_arduino_ide_bundle() -> None:
    sketch_dir = os.path.join(ROOT, "dist/Arduino_IDE_Sketch/KiterOS_Nano")
    if os.path.exists(sketch_dir):
        shutil.rmtree(sketch_dir)
    os.makedirs(sketch_dir, exist_ok=True)

    inc_dir = os.path.join(ROOT, "firmware/include")
    src_dir = os.path.join(ROOT, "firmware/src")

    # Copy all header files (.h) including the generated embedded_web.h
    for fname in os.listdir(inc_dir):
        if fname.endswith(".h"):
            shutil.copyfile(os.path.join(inc_dir, fname), os.path.join(sketch_dir, fname))

    # Copy all C++ source files and rename main.cpp to KiterOS_Nano.ino
    for fname in os.listdir(src_dir):
        if fname == "main.cpp":
            shutil.copyfile(os.path.join(src_dir, fname), os.path.join(sketch_dir, "KiterOS_Nano.ino"))
        elif fname.endswith(".cpp"):
            shutil.copyfile(os.path.join(src_dir, fname), os.path.join(sketch_dir, fname))

    # Copy filesystem assets to data/ folder for optional Arduino LittleFS upload
    data_dir = os.path.join(sketch_dir, "data")
    shutil.copytree(os.path.join(ROOT, "staging_fs"), data_dir)
    print(f"[OK] Assembled Arduino IDE sketch at {sketch_dir}")

if __name__ == "__main__":
    generate_embedded_header()
    prepare_staging_fs()
    generate_arduino_ide_bundle()
