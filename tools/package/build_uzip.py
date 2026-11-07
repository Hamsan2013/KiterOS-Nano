#!/usr/bin/env python3
"""Builds a standard ZIP-Store (.uzip) package from a source directory."""
import json
import os
import sys
import zipfile

MAX_PACKAGE_BYTES = 1_048_576  # 1 MiB
MAX_ENTRY_BYTES = 524_288       # 512 KiB

def build_uzip(src_dir: str, out_file: str) -> None:
    manifest_path = os.path.join(src_dir, "manifest.json")
    if not os.path.isfile(manifest_path):
        raise ValueError("Missing manifest.json in package root")

    with open(manifest_path, "r", encoding="utf-8") as f:
        manifest = json.load(f)

    for req in ("id", "name", "version", "type", "entry"):
        if req not in manifest:
            raise ValueError(f"manifest.json missing required field '{req}'")

    with zipfile.ZipFile(out_file, "w", compression=zipfile.ZIP_STORED) as zf:
        for root, _, files in os.walk(src_dir):
            for file in sorted(files):
                full_path = os.path.join(root, file)
                rel_path = os.path.relpath(full_path, src_dir).replace("\\", "/")
                if ".." in rel_path or rel_path.startswith("/"):
                    raise ValueError(f"Unsafe path: {rel_path}")
                if os.path.getsize(full_path) > MAX_ENTRY_BYTES:
                    raise ValueError(f"Entry {rel_path} exceeds 512 KB limit")
                zf.write(full_path, rel_path, compress_type=zipfile.ZIP_STORED)

    total = os.path.getsize(out_file)
    if total > MAX_PACKAGE_BYTES:
        os.remove(out_file)
        raise ValueError(f"Package size {total} B exceeds 1 MiB limit")
    print(f"[OK] Built {out_file} ({total} bytes)")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: build_uzip.py <source_dir> <output.uzip>")
        sys.exit(1)
    build_uzip(sys.argv[1], sys.argv[2])
