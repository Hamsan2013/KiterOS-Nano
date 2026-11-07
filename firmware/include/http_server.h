#ifndef KITEROS_HTTP_SERVER_H
#define KITEROS_HTTP_SERVER_H

#include <Arduino.h>
#include "config.h"

void initHttpServer(SystemConfig &cfg);
void processHttpServer();

#endif
