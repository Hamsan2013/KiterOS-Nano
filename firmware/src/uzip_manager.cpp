#include "uzip_manager.h"
#include "storage.h"
#include "config.h"

const char* uzipResultToString(UzipResult res) {
    switch (res) {
        case UZIP_OK:                     return "OK";
        case UZIP_ERR_TOO_LARGE:          return "Package exceeds 128 KB limit";
        case UZIP_ERR_BAD_MAGIC:          return "Invalid ZIP header signature";
        case UZIP_ERR_UNSUPPORTED_METHOD: return "Entries must use ZIP store mode (method 0)";
        case UZIP_ERR_UNSAFE_PATH:        return "Unsafe or traversal path detected";
        case UZIP_ERR_ENTRY_TOO_LARGE:    return "Entry exceeds 64 KB limit";
        case UZIP_ERR_CRC_MISMATCH:       return "CRC-32 integrity check failed";
        case UZIP_ERR_NO_MANIFEST:        return "Missing manifest.json in package";
        case UZIP_ERR_INVALID_MANIFEST:   return "Invalid manifest.json fields or version";
        case UZIP_ERR_FS_WRITE:           return "Filesystem write error";
        default:                          return "Corrupt UZIP archive";
    }
}

uint32_t crc32Update(uint32_t crc, const uint8_t *data, size_t len) {
    crc = ~crc;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t k = 0; k < 8; k++) {
            crc = (crc >> 1) ^ (0xEDB88320UL & (-(int32_t)(crc & 1)));
        }
    }
    return ~crc;
}

static uint16_t readLE16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t readLE32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static bool ensureParentDirs(const char *fullPath) {
    char temp[MAX_PATH_LEN];
    strlcpy(temp, fullPath, sizeof(temp));
    for (char *p = temp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (!LittleFS.exists(temp)) {
                LittleFS.mkdir(temp);
            }
            *p = '/';
        }
    }
    return true;
}

static bool validateManifestFile(const char *manifestPath, char *outAppId, size_t maxIdLen) {
    File f = LittleFS.open(manifestPath, "r");
    if (!f) return false;
    char buf[384];
    size_t n = f.readBytes(buf, sizeof(buf) - 1);
    buf[n] = '\0';
    f.close();

    // Must contain id, name, type, version, entry, fridge_version
    const char *idPos = strstr(buf, "\"id\"");
    if (!idPos) return false;
    const char *colon = strchr(idPos, ':');
    if (!colon) return false;
    const char *q1 = strchr(colon, '"');
    if (!q1) return false;
    q1++;
    size_t i = 0;
    while (*q1 && *q1 != '"' && i + 1 < maxIdLen) {
        outAppId[i++] = *q1++;
    }
    outAppId[i] = '\0';

    if (!isValidAppId(outAppId)) return false;
    if (!strstr(buf, "\"name\"") || !strstr(buf, "\"version\"") ||
        !strstr(buf, "\"entry\"") || !strstr(buf, "\"type\"")) {
        return false;
    }
    return true;
}

UzipResult validateAndInstallUzip(const char *archivePath, char *installedAppId, size_t appIdMaxLen) {
    File zipFile = LittleFS.open(archivePath, "r");
    if (!zipFile) return UZIP_ERR_CORRUPT_ARCHIVE;

    size_t totalSize = zipFile.size();
    if (totalSize < 30 || totalSize > MAX_UZIP_PACKAGE_SIZE) {
        zipFile.close();
        return UZIP_ERR_TOO_LARGE;
    }

    const char *stagingDir = "/tmp/stage";
    if (LittleFS.exists(stagingDir)) {
        removeDirectoryRecursive(stagingDir);
    }
    LittleFS.mkdir(stagingDir);

    bool foundManifest = false;
    uint8_t header[30];
    uint8_t chunk[STREAM_CHUNK_SIZE];

    while (zipFile.position() + 4 <= totalSize) {
        size_t pos = zipFile.position();
        if (zipFile.readBytes((char*)header, 4) != 4) {
            zipFile.close();
            removeDirectoryRecursive(stagingDir);
            return UZIP_ERR_CORRUPT_ARCHIVE;
        }

        uint32_t sig = readLE32(header);
        if (sig == 0x02014B50UL || sig == 0x06054B50UL) {
            // Reached Central Directory or End of Central Directory
            break;
        }
        if (sig != 0x04034B50UL) {
            zipFile.close();
            removeDirectoryRecursive(stagingDir);
            return UZIP_ERR_BAD_MAGIC;
        }

        if (zipFile.readBytes((char*)(header + 4), 26) != 26) {
            zipFile.close();
            removeDirectoryRecursive(stagingDir);
            return UZIP_ERR_CORRUPT_ARCHIVE;
        }

        uint16_t flags      = readLE16(header + 6);
        uint16_t method     = readLE16(header + 8);
        uint32_t expectedCrc= readLE32(header + 14);
        uint32_t compSize   = readLE32(header + 18);
        uint32_t uncompSize = readLE32(header + 22);
        uint16_t nameLen    = readLE16(header + 26);
        uint16_t extraLen   = readLE16(header + 28);

        // Reject data descriptor streaming bit (bit 3) or non-store method
        if ((flags & 0x0008) != 0 || method != 0 || compSize != uncompSize) {
            zipFile.close();
            removeDirectoryRecursive(stagingDir);
            return UZIP_ERR_UNSUPPORTED_METHOD;
        }
        if (uncompSize > MAX_UZIP_ENTRY_SIZE || nameLen == 0 || nameLen >= 64) {
            zipFile.close();
            removeDirectoryRecursive(stagingDir);
            return UZIP_ERR_ENTRY_TOO_LARGE;
        }

        char entryName[64];
        if (zipFile.readBytes(entryName, nameLen) != nameLen) {
            zipFile.close();
            removeDirectoryRecursive(stagingDir);
            return UZIP_ERR_CORRUPT_ARCHIVE;
        }
        entryName[nameLen] = '\0';

        if (extraLen > 0) {
            zipFile.seek(pos + 30 + nameLen + extraLen, SeekSet);
        }

        // Skip directory-only entries
        if (entryName[nameLen - 1] == '/') {
            continue;
        }

        if (!isSafeRelativePath(entryName)) {
            zipFile.close();
            removeDirectoryRecursive(stagingDir);
            return UZIP_ERR_UNSAFE_PATH;
        }

        char destPath[MAX_PATH_LEN];
        snprintf(destPath, sizeof(destPath), "%s/%s", stagingDir, entryName);
        ensureParentDirs(destPath);

        File outFile = LittleFS.open(destPath, "w");
        if (!outFile) {
            zipFile.close();
            removeDirectoryRecursive(stagingDir);
            return UZIP_ERR_FS_WRITE;
        }

        uint32_t remaining = uncompSize;
        uint32_t calcCrc = 0;
        while (remaining > 0) {
            size_t toRead = (remaining > STREAM_CHUNK_SIZE) ? STREAM_CHUNK_SIZE : remaining;
            size_t got = zipFile.readBytes((char*)chunk, toRead);
            if (got != toRead) {
                outFile.close();
                zipFile.close();
                removeDirectoryRecursive(stagingDir);
                return UZIP_ERR_CORRUPT_ARCHIVE;
            }
            calcCrc = crc32Update(calcCrc, chunk, got);
            if (outFile.write(chunk, got) != got) {
                outFile.close();
                zipFile.close();
                removeDirectoryRecursive(stagingDir);
                return UZIP_ERR_FS_WRITE;
            }
            remaining -= got;
            yield();
        }
        outFile.close();

        if (calcCrc != expectedCrc) {
            zipFile.close();
            removeDirectoryRecursive(stagingDir);
            return UZIP_ERR_CRC_MISMATCH;
        }

        if (strcmp(entryName, "manifest.json") == 0) {
            foundManifest = true;
        }
    }
    zipFile.close();

    if (!foundManifest) {
        removeDirectoryRecursive(stagingDir);
        return UZIP_ERR_NO_MANIFEST;
    }

    char stageManifest[MAX_PATH_LEN];
    snprintf(stageManifest, sizeof(stageManifest), "%s/manifest.json", stagingDir);
    if (!validateManifestFile(stageManifest, installedAppId, appIdMaxLen)) {
        removeDirectoryRecursive(stagingDir);
        return UZIP_ERR_INVALID_MANIFEST;
    }

    char finalAppDir[MAX_PATH_LEN];
    snprintf(finalAppDir, sizeof(finalAppDir), "/apps/%s", installedAppId);
    if (LittleFS.exists(finalAppDir)) {
        removeDirectoryRecursive(finalAppDir);
    }

    if (!LittleFS.rename(stagingDir, finalAppDir)) {
        removeDirectoryRecursive(stagingDir);
        return UZIP_ERR_FS_WRITE;
    }

    return UZIP_OK;
}

bool uninstallAppPackage(const char *appId) {
    if (!isValidAppId(appId)) return false;
    char appDir[MAX_PATH_LEN];
    snprintf(appDir, sizeof(appDir), "/apps/%s", appId);
    if (!LittleFS.exists(appDir)) return false;
    return removeDirectoryRecursive(appDir);
}
