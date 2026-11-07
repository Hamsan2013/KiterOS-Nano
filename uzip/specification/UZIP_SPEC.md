# UZIP Package Format Specification (v1.0)

1. Container: Standard PKZIP archive (`PK\x03\x04` local file headers) using `.uzip` extension.
2. Compression Method: Store mode (`method = 0`, uncompressed) so the ESP8266 can validate CRC-32 checksums and stream-extract entries in 512-byte chunks without allocating a 32 KB LZ77 sliding window.
3. Limits:
   - Maximum archive size: `1,048,576` bytes (1 MiB).
   - Maximum single entry size: `524,288` bytes (512 KiB).
   - Relative paths only; any entry containing `..`, leading `/`, or `\` is rejected immediately.
4. Required Root File: `manifest.json` containing `id`, `name`, `version`, `type` (`"app"` or `"game"`), `entry` (`"source/main.frg"`), and `fridge_version`.
