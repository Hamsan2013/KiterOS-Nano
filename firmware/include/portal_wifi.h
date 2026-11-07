#ifndef KITEROS_PORTAL_WIFI_H
#define KITEROS_PORTAL_WIFI_H

#include <Arduino.h>
#include "config.h"

bool startSoftAPAndPortal(const SystemConfig &cfg);
void processPortalDns();

#endif
