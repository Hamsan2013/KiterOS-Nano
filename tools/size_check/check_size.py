  #!/usr/bin/env python3
"""
Verifies firmware.bin and filesystem.bin partition limits and enforces the
strict 1,048,576-byte (1 MiB) combined installed size limit.
"""
import gzip
import os
import sys

MAX_COMBINED_INSTALLED_BYTES = 1_048_576  # 1 MiB strict limit
MAX_FIRMWARE_PARTITION_BYTES = 458_752    # 448 KiB budget (leaves >550 KB OTA space)
MAX_FILESYSTEM_IMAGE_BYTES   = 524_288    # 512 KiB LittleFS image budget

def main():
    if len(sys.argv) < 4:
        print("Usage: check_size.py <firmware.bin> <filesystem.bin> <report.txt>")
        sys.exit(1)

    fw_path, fs_path, report_path = sys.argv[1], sys.argv[2], sys.argv[3]
    fw_size = os.path.getsize(fw_path)
    fs_size = os.path.getsize(fs_path)
    combined_installed = fw_size + fs_size

    fw_gz_size = len(gzip.compress(open(fw_path, "rb").read()))
    fs_gz_size = len(gzip.compress(open(fs_path, "rb").read()))
    combined_compressed = fw_gz_size + fs_gz_size

    report = [
        "==============================================================",
        "               KiterOS Nano Build Size Report                 ",
        "==============================================================",
        f"Firmware Binary (Installed)    : {fw_size:>9,} bytes ({fw_size / 1024:.2f} KiB)",
        f"LittleFS Image  (Installed)    : {fs_size:>9,} bytes ({fs_size / 1024:.2f} KiB)",
        "--------------------------------------------------------------",
        f"COMBINED INSTALLED SIZE        : {combined_installed:>9,} bytes ({combined_installed / 1024:.2f} KiB)",
        f"STRICT 1 MiB CEILING           : {MAX_COMBINED_INSTALLED_BYTES:>9,} bytes (1024.00 KiB)",
        f"REMAINING 1 MiB MARGIN         : {MAX_COMBINED_INSTALLED_BYTES - combined_installed:>9,} bytes",
        "--------------------------------------------------------------",
        f"Compressed Download (GZIP Est.): {combined_compressed:>9,} bytes ({combined_compressed / 1024:.2f} KiB)",
        "==============================================================",
    ]
    report_text = "\n".join(report) + "\n"
    print(report_text)

    with open(report_path, "w", encoding="utf-8") as f:
        f.write(report_text)

    if fw_size > MAX_FIRMWARE_PARTITION_BYTES:
        print(f"[FAIL] Firmware size {fw_size} B exceeds budget {MAX_FIRMWARE_PARTITION_BYTES} B")
        sys.exit(1)
    if fs_size > MAX_FILESYSTEM_IMAGE_BYTES:
        print(f"[FAIL] Filesystem image {fs_size} B exceeds budget {MAX_FILESYSTEM_IMAGE_BYTES} B")
        sys.exit(1)
    if combined_installed > MAX_COMBINED_INSTALLED_BYTES:
        print(f"[FAIL] Combined installed size {combined_installed} B exceeds 1 MiB (1,048,576 B)!")
        sys.exit(1)

    print("[PASS] All partition and 1 MiB combined installation limits satisfied.")

if __name__ == "__main__":
    main()
