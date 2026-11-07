const char* uzipResultToString(UzipResult res) {
    switch (res) {
        case UZIP_OK:                     return "OK";
        case UZIP_ERR_TOO_LARGE:          return "Violation: .uzip exceeds strict 1 KB (1024 bytes) limit";
        case UZIP_ERR_BAD_MAGIC:          return "Invalid UZIP/ZIP header signature";
        case UZIP_ERR_UNSUPPORTED_METHOD: return "Entries must use UZIP store wrapper (pre-gzipped files)";
        case UZIP_ERR_UNSAFE_PATH:        return "Unsafe or traversal path detected";
        case UZIP_ERR_ENTRY_TOO_LARGE:    return "Entry exceeds 1 KB limit";
        case UZIP_ERR_CRC_MISMATCH:       return "CRC-32 integrity check failed";
        case UZIP_ERR_NO_MANIFEST:        return "Missing manifest.json in package";
        case UZIP_ERR_INVALID_MANIFEST:   return "Invalid manifest.json fields";
        case UZIP_ERR_FS_WRITE:           return "LittleFS write error";
        default:                          return "Corrupt .uzip archive";
    }
}
