#!/usr/bin/env python3
"""Automated UZIP package validation, path traversal, and size limit tests."""
import json
import os
import tempfile
import unittest
import zipfile

MAX_PKG = 1_048_576
MAX_ENTRY = 524_288

def validate_uzip_archive(zip_path: str):
    if os.path.getsize(zip_path) > MAX_PKG:
        return False, "Package exceeds 1 MiB limit"
    with zipfile.ZipFile(zip_path, "r") as zf:
        has_manifest = False
        for info in zf.infolist():
            if info.compress_type != zipfile.ZIP_STORED:
                return False, "Unsupported compression method"
            if ".." in info.filename or info.filename.startswith("/") or "\\" in info.filename:
                return False, "Unsafe path traversal detected"
            if info.file_size > MAX_ENTRY:
                return False, "Entry exceeds 512 KB limit"
            if info.filename == "manifest.json":
                has_manifest = True
                data = json.loads(zf.read(info.filename).decode("utf-8"))
                for k in ("id", "name", "version", "type", "entry"):
                    if k not in data:
                        return False, f"Missing manifest key {k}"
        if not has_manifest:
            return False, "Missing manifest.json"
    return True, "OK"

class TestUzipValidation(unittest.TestCase):
    def test_valid_package(self):
        with tempfile.NamedTemporaryFile(suffix=".uzip", delete=False) as tf:
            path = tf.name
        try:
            with zipfile.ZipFile(path, "w", zipfile.ZIP_STORED) as zf:
                zf.writestr("manifest.json", '{"id":"hello","name":"Hello","version":"1.0","type":"app","entry":"source/main.frg"}')
                zf.writestr("source/main.frg", 'app "Hello"\n')
            ok, msg = validate_uzip_archive(path)
            self.assertTrue(ok, msg)
        finally:
            os.remove(path)

    def test_reject_path_traversal(self):
        with tempfile.NamedTemporaryFile(suffix=".uzip", delete=False) as tf:
            path = tf.name
        try:
            with zipfile.ZipFile(path, "w", zipfile.ZIP_STORED) as zf:
                zf.writestr("manifest.json", '{"id":"bad","name":"Bad","version":"1.0","type":"app","entry":"main.frg"}')
                zf.writestr("../../etc/passwd", "root:x:0:0")
            ok, msg = validate_uzip_archive(path)
            self.assertFalse(ok)
            self.assertIn("traversal", msg)
        finally:
            os.remove(path)

    def test_reject_oversized_entry(self):
        with tempfile.NamedTemporaryFile(suffix=".uzip", delete=False) as tf:
            path = tf.name
        try:
            with zipfile.ZipFile(path, "w", zipfile.ZIP_STORED) as zf:
                zf.writestr("manifest.json", '{"id":"big","name":"Big","version":"1.0","type":"app","entry":"main.frg"}')
                zf.writestr("huge.bin", b"A" * (MAX_ENTRY + 10))
            ok, msg = validate_uzip_archive(path)
            self.assertFalse(ok)
            self.assertIn("512 KB", msg)
        finally:
            os.remove(path)

if __name__ == "__main__":
    unittest.main()
