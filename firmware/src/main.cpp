#include <Arduino.h>
#include "config.h"
#include "sys_info.h"
#include "storage.h"
#include "portal_wifi.h"
#include "http_server.h"

static SystemConfig sysConfig;

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println();
    Serial.println(F("===================================================="));
    Serial.println(F("             KiterOS Nano v0.1.0 Boot               "));
    Serial.println(F("===================================================="));

    if (!initStorage()) {
        Serial.println(F("[FATAL] Storage mount failed."));
    } else {
        Serial.println(F("[OK] LittleFS mounted."));
    }

    loadSystemConfig(sysConfig);

    if (startSoftAPAndPortal(sysConfig)) {
        Serial.printf("[OK] SoftAP '%s' started at 192.168.4.1\n", sysConfig.ssid);
    }

    initHttpServer(sysConfig);
    Serial.println(F("[OK] HTTP Server + Captive Portal active on port 80."));

    HardwareMetrics m;
    collectHardwareMetrics(m);
    Serial.printf("Chip ID: 0x%05x | Sketch: %u B | Free OTA: %u B | Heap: %u B\n",
                  m.chipId, m.sketchSize, m.freeSketchSpace, m.freeHeap);
    Serial.println(F("===================================================="));
}

void loop() {
    processPortalDns();
    processHttpServer();
}
