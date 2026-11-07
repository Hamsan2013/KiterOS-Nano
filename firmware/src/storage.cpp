#include "storage.h"
#include "config.h"

bool initStorage() {
    if (!LittleFS.begin()) {
        Serial.println(F("[WARN] LittleFS mount failed. Formatting..."));
        if (!LittleFS.format() || !LittleFS.begin()) {
            Serial.println(F("[ERR] LittleFS initialization failed!"));
            return false;
        }
    }
    if (!LittleFS.exists("/apps")) {
        LittleFS.mkdir("/apps");
    }
    if (!LittleFS.exists("/tmp")) {
        LittleFS.mkdir("/tmp");
    }
    if (!LittleFS.exists("/config")) {
        LittleFS.mkdir("/config");
    }
    return true;
}

bool isSafeRelativePath(const char *path) {
    if (!path || path[0] == '\0' || path[0] == '/' || path[0] == '\\') return false;
    size_t len = strlen(path);
    if (len == 0 || len >= MAX_PATH_LEN) return false;

    for (size_t i = 0; i < len; i++) {
        char c = path[i];
        if (c == '\\' || c < 32 || c > 126) return false;
        if (c == '.' && i + 1 < len && path[i + 1] == '.') return false;
    }
    return true;
}

bool isValidAppId(const char *appId) {
    if (!appId || appId[0] == '\0') return false;
    size_t len = strlen(appId);
    if (len < 2 || len > 24) return false;
    for (size_t i = 0; i < len; i++) {
        char c = appId[i];
        bool ok = (c >= 'a' && c <= 'z') ||
                  (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') ||
                  (c == '-' || c == '_');
        if (!ok) return false;
    }
    return true;
}

bool removeDirectoryRecursive(const char *dirPath) {
    Dir dir = LittleFS.openDir(dirPath);
    while (dir.next()) {
        char childPath[MAX_PATH_LEN];
        snprintf(childPath, sizeof(childPath), "%s/%s", dirPath, dir.fileName().c_str());
        if (dir.isDirectory()) {
            removeDirectoryRecursive(childPath);
        } else {
            LittleFS.remove(childPath);
        }
        yield();
    }
    return LittleFS.rmdir(dirPath);
}

static bool extractJsonField(const char *json, const char *key, char *outVal, size_t maxLen) {
    char pattern[32];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *p = strstr(json, pattern);
    if (!p) return false;
    p = strchr(p + strlen(pattern), ':');
    if (!p) return false;
    p++;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (*p != '"') return false;
    p++;
    size_t i = 0;
    while (*p && *p != '"' && i + 1 < maxLen) {
        outVal[i++] = *p++;
    }
    outVal[i] = '\0';
    return i > 0;
}

bool loadSystemConfig(SystemConfig &cfg) {
    strlcpy(cfg.ssid, DEFAULT_AP_SSID, sizeof(cfg.ssid));
    strlcpy(cfg.password, DEFAULT_AP_PASS, sizeof(cfg.password));
    cfg.channel = DEFAULT_AP_CHANNEL;

    if (!LittleFS.exists("/config/system.json")) {
        return saveSystemConfig(cfg);
    }
    File f = LittleFS.open("/config/system.json", "r");
    if (!f) return false;
    char buf[256];
    size_t n = f.readBytes(buf, sizeof(buf) - 1);
    buf[n] = '\0';
    f.close();

    char tmpSsid[33];
    char tmpPass[65];
    if (extractJsonField(buf, "ssid", tmpSsid, sizeof(tmpSsid)) && strlen(tmpSsid) >= 3) {
        strlcpy(cfg.ssid, tmpSsid, sizeof(cfg.ssid));
    }
    if (extractJsonField(buf, "password", tmpPass, sizeof(tmpPass)) && strlen(tmpPass) >= 8) {
        strlcpy(cfg.password, tmpPass, sizeof(cfg.password));
    }
    return true;
}

bool saveSystemConfig(const SystemConfig &cfg) {
    if (strlen(cfg.ssid) < 3 || strlen(cfg.password) < 8) return false;
    File f = LittleFS.open("/config/system.json", "w");
    if (!f) return false;
    char buf[200];
    snprintf_P(buf, sizeof(buf),
               PSTR("{\"ssid\":\"%s\",\"password\":\"%s\",\"channel\":%u}"),
               cfg.ssid, cfg.password, cfg.channel);
    f.print(buf);
    f.close();
    return true;
}

size_t listInstalledPackagesJson(char *outBuf, size_t maxLen) {
    size_t pos = 0;
    pos += snprintf(outBuf + pos, maxLen - pos, "{\"apps\":[");

    Dir appsDir = LittleFS.openDir("/apps");
    bool first = true;
    while (appsDir.next()) {
        if (!appsDir.isDirectory()) continue;
        char manifestPath[MAX_PATH_LEN];
        snprintf(manifestPath, sizeof(manifestPath), "/apps/%s/manifest.json", appsDir.fileName().c_str());
        if (!LittleFS.exists(manifestPath)) continue;

        File mf = LittleFS.open(manifestPath, "r");
        if (!mf) continue;
        char mBuf[384];
        size_t n = mf.readBytes(mBuf, sizeof(mBuf) - 1);
        mBuf[n] = '\0';
        mf.close();

        if (pos + n + 4 >= maxLen) break;
        if (!first) outBuf[pos++] = ',';
        first = false;
        memcpy(outBuf + pos, mBuf, n);
        pos += n;
        yield();
    }
    pos += snprintf(outBuf + pos, maxLen - pos, "]}");
    return pos;
}
