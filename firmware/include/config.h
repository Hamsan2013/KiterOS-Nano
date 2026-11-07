#ifndef KITEROS_CONFIG_H
#define KITEROS_CONFIG_H

#include <Arduino.h>

#define KITEROS_VERSION       "0.2.0"
#define DEFAULT_AP_SSID       "KiterOS-Nano"
#define DEFAULT_AP_PASS       "kiteros8266"
#define DEFAULT_AP_CHANNEL    1
#define DEFAULT_MAX_CLIENTS   4

// STRICT UZIP RULE: Every .uzip package must be <= 1,024 bytes (1 KB)
#define MAX_UZIP_PACKAGE_SIZE (1024UL)
#define MAX_UZIP_ENTRY_SIZE   (1024UL)
#define MAX_PATH_LEN          96
#define STREAM_CHUNK_SIZE     256

struct SystemConfig {
    char ssid[33];
    char password[65];
    uint8_t channel;
};

bool loadSystemConfig(SystemConfig &cfg);
bool saveSystemConfig(const SystemConfig &cfg);

#endif
