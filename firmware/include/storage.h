#ifndef KITEROS_STORAGE_H
#define KITEROS_STORAGE_H

#include <Arduino.h>
#include <LittleFS.h>

bool initStorage();
bool isSafeRelativePath(const char *path);
bool isValidAppId(const char *appId);
bool removeDirectoryRecursive(const char *dirPath);
size_t listInstalledPackagesJson(char *outBuf, size_t maxLen);

#endif
