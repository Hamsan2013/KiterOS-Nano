#!/usr/bin/env python3
"""
Compiles index.html, style.css, app.js, and fridge.js directly into
firmware/include/embedded_web.h (PROGMEM) so the UI is built into firmware.bin
and can never be missing on boot.
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
    with open(header_path, "w", encoding="utf-8") as f:
        f.write("#ifndef KITEROS_EMBEDDED_WEB_H\n#define KITEROS_EMBEDDED_WEB_H\n\n")
        f.write("#include <Arduino.h>\n\n")
        f.write(to_c_array("EMBED_INDEX_HTML_GZ", read_bytes("filesystem/launcher/index.html")))
        f.write(to_c_array("EMBED_STYLE_CSS_GZ",  read_bytes("filesystem/launcher/style.css")))
        f.write(to_c_array("EMBED_APP_JS_GZ",     read_bytes("filesystem/launcher/app.js")))
        f.write(to_c_array("EMBED_FRIDGE_JS_GZ",  read_bytes("filesystem/runtime/fridge.js")))
        f.write("#endif\n")
    print(f"[OK] Generated boot-squashed PROGMEM header: {header_path}")

def prepare_staging_fs() -> None:
    staging = os.path.join(ROOT, "staging_fs")
    if os.path.exists(staging):
        shutil.rmtree(staging)
    os.makedirs(os.path.join(staging, "config"), exist_ok=True)
    os.makedirs(os.path.join(staging, "apps"), exist_ok=True)
    shutil.copyfile(
        os.path.join(ROOT, "filesystem/config/system.json"),
        os.path.join(staging, "config/system.json")
    )

def generate_arduino_ide_sketch() -> None:
    out_dir = os.path.join(ROOT, "dist/Arduino_IDE_Sketch/KiterOS_Nano")
    os.makedirs(out_dir, exist_ok=True)
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
    print(f"[OK] Generated single-flash Arduino IDE sketch at: {out_dir}")

if __name__ == "__main__":
    generate_embedded_header()
    prepare_staging_fs()
    generate_arduino_ide_sketch()
