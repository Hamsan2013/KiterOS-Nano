#include "portal_wifi.h"
#include <ESP8266WiFi.h>
#include <DNSServer.h>

static DNSServer dnsServer;
static const byte DNS_PORT = 53;
static const IPAddress AP_IP(192, 168, 4, 1);
static const IPAddress AP_GW(192, 168, 4, 1);
static const IPAddress AP_MASK(255, 255, 255, 0);

bool startSoftAPAndPortal(const SystemConfig &cfg) {
    WiFi.persistent(false);
    WiFi.disconnect(true);
    WiFi.mode(WIFI_AP);

    if (!WiFi.softAPConfig(AP_IP, AP_GW, AP_MASK)) {
        Serial.println(F("[ERR] SoftAP IP config failed"));
        return false;
    }

    if (!WiFi.softAP(cfg.ssid, cfg.password, cfg.channel, false, DEFAULT_MAX_CLIENTS)) {
        Serial.println(F("[ERR] SoftAP start failed"));
        return false;
    }

    dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    dnsServer.start(DNS_PORT, "*", AP_IP);
    return true;
}

void processPortalDns() {
    dnsServer.processNextRequest();
}
