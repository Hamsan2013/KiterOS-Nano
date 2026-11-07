#ifndef KITEROS_CONFIG_H
#define KITEROS_CONFIG_H

#include <Arduino.h>

#define KITEROS_VERSION       "0.1.0"
#define DEFAULT_AP_SSID       "KiterOS-Nano"
#define DEFAULT_AP_PASS       "kiteros8266"
#define DEFAULT_AP_CHANNEL    1
#define DEFAULT_MAX_CLIENTS   4

#define MAX_UZIP_PACKAGE_SIZE (131072UL) // 128 KB max per package
#define MAX_UZIP_ENTRY_SIZE   (65536UL)  // 64 KB max per single file
#define MAX_PATH_LEN          96
#define STREAM_CHUNK_SIZE     512

struct SystemConfig {
    char ssid[33];
    char password[65];
    uint8_t channel;
};

bool loadSystemConfig(SystemConfig &cfg);
bool saveSystemConfig(const SystemConfig &cfg);

#endif
