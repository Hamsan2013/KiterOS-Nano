#include "sys_info.h"
#include "config.h"
#include <ESP8266WiFi.h>
#include <LittleFS.h>

void collectHardwareMetrics(HardwareMetrics &m) {
    m.chipId            = ESP.getChipId();
    m.flashSize         = ESP.getFlashChipRealSize();
    m.sketchSize        = ESP.getSketchSize();
    m.freeSketchSpace   = ESP.getFreeSketchSpace();
    m.freeHeap          = ESP.getFreeHeap();
    m.heapFragmentation = ESP.getHeapFragmentation();
    m.cpuFreqMHz        = ESP.getCpuFreqMHz();
    m.uptimeSeconds     = millis() / 1000UL;
    m.connectedStations = WiFi.softAPgetStationNum();

    FSInfo fs_info;
    if (LittleFS.info(fs_info)) {
        m.fsTotalBytes = fs_info.totalBytes;
        m.fsUsedBytes  = fs_info.usedBytes;
    } else {
        m.fsTotalBytes = 0;
        m.fsUsedBytes  = 0;
    }
}

size_t formatSystemStatusJson(char *buf, size_t maxLen, const char *ssid) {
    HardwareMetrics m;
    collectHardwareMetrics(m);
    IPAddress ip = WiFi.softAPIP();

    int written = snprintf_P(
        buf, maxLen,
        PSTR("{\"os\":\"KiterOS Nano\",\"version\":\"%s\",\"chip_id\":\"0x%05x\","
             "\"cpu_mhz\":%u,\"flash_bytes\":%u,\"sketch_bytes\":%u,\"ota_free_bytes\":%u,"
             "\"free_heap\":%u,\"heap_frag\":%u,\"fs_total\":%u,\"fs_used\":%u,"
             "\"uptime_s\":%u,\"ssid\":\"%s\",\"ip\":\"%d.%d.%d.%d\",\"clients\":%u}"),
        KITEROS_VERSION,
        m.chipId,
        m.cpuFreqMHz,
        m.flashSize,
        m.sketchSize,
        m.freeSketchSpace,
        m.freeHeap,
        m.heapFragmentation,
        m.fsTotalBytes,
        m.fsUsedBytes,
        m.uptimeSeconds,
        ssid,
        ip[0], ip[1], ip[2], ip[3],
        m.connectedStations
    );
    return (written > 0 && (size_t)written < maxLen) ? (size_t)written : 0;
}
