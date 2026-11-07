#ifndef KITEROS_UZIP_MANAGER_H
#define KITEROS_UZIP_MANAGER_H

#include <Arduino.h>
#include <LittleFS.h>

enum UzipResult {
    UZIP_OK = 0,
    UZIP_ERR_TOO_LARGE,
    UZIP_ERR_BAD_MAGIC,
    UZIP_ERR_UNSUPPORTED_METHOD,
    UZIP_ERR_UNSAFE_PATH,
    UZIP_ERR_ENTRY_TOO_LARGE,
    UZIP_ERR_CRC_MISMATCH,
    UZIP_ERR_NO_MANIFEST,
    UZIP_ERR_INVALID_MANIFEST,
    UZIP_ERR_FS_WRITE,
    UZIP_ERR_CORRUPT_ARCHIVE
};

const char* uzipResultToString(UzipResult res);
uint32_t crc32Update(uint32_t crc, const uint8_t *data, size_t len);
UzipResult validateAndInstallUzip(const char *archivePath, char *installedAppId, size_t appIdMaxLen);
bool uninstallAppPackage(const char *appId);

#endif
