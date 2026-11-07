#ifndef KITEROS_SYS_INFO_H
#define KITEROS_SYS_INFO_H

#include <Arduino.h>

struct HardwareMetrics {
    uint32_t chipId;
    uint32_t flashSize;
    uint32_t sketchSize;
    uint32_t freeSketchSpace;
    uint32_t freeHeap;
    uint8_t  heapFragmentation;
    uint32_t cpuFreqMHz;
    uint32_t fsTotalBytes;
    uint32_t fsUsedBytes;
    uint32_t uptimeSeconds;
    uint8_t  connectedStations;
};

void collectHardwareMetrics(HardwareMetrics &m);
size_t formatSystemStatusJson(char *buf, size_t maxLen, const char *ssid);

#endif
