#include "http_server.h"
#include "sys_info.h"
#include "storage.h"
#include "uzip_manager.h"
#include "embedded_web.h"
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <LittleFS.h>

static ESP8266WebServer server(80);
static ESP8266HTTPUpdateServer httpUpdater;
static SystemConfig *activeCfg = nullptr;
static File uploadFile;
static bool uploadError = false;
static size_t uploadedBytes = 0;

// Fallback 1x1 Cyan PNG icon if a package does not include icon.png
static const uint8_t DEFAULT_ICON_PNG[] PROGMEM = {
    0x89,0x50,0x4E,0x47,0x0D,0x0A,0x1A,0x0A,0x00,0x00,0x00,0x0D,0x49,0x48,0x44,0x52,
    0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x01,0x08,0x02,0x00,0x00,0x00,0x90,0x77,0x53,
    0xDE,0x00,0x00,0x00,0x0C,0x49,0x44,0x41,0x54,0x08,0xD7,0x63,0x60,0xF8,0xFF,0x1F,
    0x00,0x02,0xFE,0x01,0xFE,0xA7,0xC2,0x89,0x89,0x00,0x00,0x00,0x00,0x49,0x45,0x4E,
    0x44,0xAE,0x42,0x60,0x82
};

static const char* getMimeType(const char *path) {
    if (strstr(path, ".html")) return "text/html";
    if (strstr(path, ".css"))  return "text/css";
    if (strstr(path, ".js"))   return "application/javascript";
    if (strstr(path, ".json")) return "application/json";
    if (strstr(path, ".frg"))  return "text/plain";
    if (strstr(path, ".png"))  return "image/png";
    if (strstr(path, ".svg"))  return "image/svg+xml";
    return "application/octet-stream";
}

// Sends a boot-squashed PROGMEM gzip asset so the UI is NEVER missing
static void sendEmbeddedGzip(const char *mime, const uint8_t *data, size_t len) {
    server.sendHeader("Content-Encoding", "gzip");
    server.sendHeader("Cache-Control", "no-cache");
    server.send_P(200, mime, (const char*)data, len);
}

static bool serveAppFileFromFS(const char *fsPath) {
    char gzPath[MAX_PATH_LEN];
    snprintf(gzPath, sizeof(gzPath), "%s.gz", fsPath);
    if (LittleFS.exists(gzPath)) {
        File f = LittleFS.open(gzPath, "r");
        if (f) {
            server.sendHeader("Cache-Control", "no-cache");
            server.streamFile(f, getMimeType(fsPath));
            f.close();
            return true;
        }
    }
    if (LittleFS.exists(fsPath)) {
        File f = LittleFS.open(fsPath, "r");
        if (f) {
            server.sendHeader("Cache-Control", "no-cache");
            server.streamFile(f, getMimeType(fsPath));
            f.close();
            return true;
        }
    }
    return false;
}

// Force Android / iOS / Windows to automatically pop open the Captive Portal browser
static void handleCaptiveRedirect() {
    server.sendHeader("Location", "http://192.168.4.1/", true);
    server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    server.send(302, "text/html",
        "<!DOCTYPE html><html><head><meta http-equiv='refresh' content='0;url=http://192.168.4.1/'></head>"
        "<body><a href='http://192.168.4.1/'>Open KiterOS Nano</a></body></html>");
}

static void handleApiStatus() {
    char buf[384];
    formatSystemStatusJson(buf, sizeof(buf), activeCfg ? activeCfg->ssid : DEFAULT_AP_SSID);
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", buf);
}

static void handleApiApps() {
    char buf[1536];
    listInstalledPackagesJson(buf, sizeof(buf));
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", buf);
}

static void handleApiAppIcon() {
    if (!server.hasArg("id") || !isValidAppId(server.arg("id").c_str())) {
        server.send_P(200, "image/png", (const char*)DEFAULT_ICON_PNG, sizeof(DEFAULT_ICON_PNG));
        return;
    }
    char iconPath[MAX_PATH_LEN];
    snprintf(iconPath, sizeof(iconPath), "/apps/%s/icon.png", server.arg("id").c_str());
    if (!serveAppFileFromFS(iconPath)) {
        server.send_P(200, "image/png", (const char*)DEFAULT_ICON_PNG, sizeof(DEFAULT_ICON_PNG));
    }
}

static void handleApiAppFile() {
    if (!server.hasArg("id") || !server.hasArg("path")) {
        server.send(400, "application/json", "{\"error\":\"Missing id or path\"}");
        return;
    }
    String id = server.arg("id");
    String rel = server.arg("path");
    if (!isValidAppId(id.c_str()) || !isSafeRelativePath(rel.c_str())) {
        server.send(403, "application/json", "{\"error\":\"Forbidden path\"}");
        return;
    }
    char fullPath[MAX_PATH_LEN];
    snprintf(fullPath, sizeof(fullPath), "/apps/%s/%s", id.c_str(), rel.c_str());
    if (!serveAppFileFromFS(fullPath)) {
        server.send(404, "application/json", "{\"error\":\"File not found\"}");
    }
}

static void appendDirFilesJson(const char *baseAppDir, const char *subDir, char *out, size_t maxLen, size_t &pos, bool &first) {
    char curDir[MAX_PATH_LEN];
    if (subDir[0] == '\0') {
        strlcpy(curDir, baseAppDir, sizeof(curDir));
    } else {
        snprintf(curDir, sizeof(curDir), "%s/%s", baseAppDir, subDir);
    }

    Dir d = LittleFS.openDir(curDir);
    while (d.next()) {
        String fn = d.fileName();
        if (fn.endsWith(".gz")) {
            fn = fn.substring(0, fn.length() - 3);
        }
        char relPath[MAX_PATH_LEN];
        if (subDir[0] == '\0') {
            strlcpy(relPath, fn.c_str(), sizeof(relPath));
        } else {
            snprintf(relPath, sizeof(relPath), "%s/%s", subDir, fn.c_str());
        }
        if (d.isDirectory()) {
            appendDirFilesJson(baseAppDir, relPath, out, maxLen, pos, first);
        } else {
            if (pos + strlen(relPath) + 8 >= maxLen) break;
            if (!first) out[pos++] = ',';
            first = false;
            pos += snprintf(out + pos, maxLen - pos, "\"%s\"", relPath);
        }
        yield();
    }
}

static void handleApiAppBundleList() {
    if (!server.hasArg("id") || !isValidAppId(server.arg("id").c_str())) {
        server.send(400, "application/json", "{\"error\":\"Invalid app id\"}");
        return;
    }
    char appDir[MAX_PATH_LEN];
    snprintf(appDir, sizeof(appDir), "/apps/%s", server.arg("id").c_str());
    if (!LittleFS.exists(appDir)) {
        server.send(404, "application/json", "{\"error\":\"App not found\"}");
        return;
    }

    char out[768];
    size_t pos = 0;
    bool first = true;
    pos += snprintf(out + pos, sizeof(out) - pos, "{\"id\":\"%s\",\"files\":[", server.arg("id").c_str());
    appendDirFilesJson(appDir, "", out, sizeof(out), pos, first);
    pos += snprintf(out + pos, sizeof(out) - pos, "]}");
    server.send(200, "application/json", out);
}

static void handleUzipUploadData() {
    HTTPUpload &upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
        uploadError = false;
        uploadedBytes = 0;
        if (LittleFS.exists("/tmp/upload.uzip")) {
            LittleFS.remove("/tmp/upload.uzip");
        }
        uploadFile = LittleFS.open("/tmp/upload.uzip", "w");
        if (!uploadFile) uploadError = true;
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (!uploadError && uploadFile) {
            uploadedBytes += upload.currentSize;
            if (uploadedBytes > MAX_UZIP_PACKAGE_SIZE) {
                uploadError = true;
            } else if (uploadFile.write(upload.buf, upload.currentSize) != upload.currentSize) {
                uploadError = true;
            }
        }
        yield();
    } else if (upload.status == UPLOAD_FILE_END || upload.status == UPLOAD_FILE_ABORTED) {
        if (uploadFile) uploadFile.close();
        if (upload.status == UPLOAD_FILE_ABORTED) uploadError = true;
    }
}

static void handleUzipUploadComplete() {
    if (uploadError || uploadedBytes == 0 || uploadedBytes > MAX_UZIP_PACKAGE_SIZE) {
        LittleFS.remove("/tmp/upload.uzip");
        server.send(400, "application/json", "{\"error\":\"Rejected: .uzip file must be <= 1 MB (1,048,576 bytes)\"}");
        return;
    }

    char appId[32] = {0};
    UzipResult res = validateAndInstallUzip("/tmp/upload.uzip", appId, sizeof(appId));
    LittleFS.remove("/tmp/upload.uzip");

    if (res != UZIP_OK) {
        char errBuf[128];
        snprintf(errBuf, sizeof(errBuf), "{\"error\":\"%s\"}", uzipResultToString(res));
        server.send(400, "application/json", errBuf);
        return;
    }

    char okBuf[112];
    snprintf(okBuf, sizeof(okBuf), "{\"status\":\"installed\",\"id\":\"%s\",\"bytes\":%u}", appId, (unsigned int)uploadedBytes);
    server.send(200, "application/json", okBuf);
}

static void handleApiUninstall() {
    if (!server.hasArg("id") || !uninstallAppPackage(server.arg("id").c_str())) {
        server.send(400, "application/json", "{\"error\":\"Failed to uninstall package\"}");
        return;
    }
    server.send(200, "application/json", "{\"status\":\"uninstalled\"}");
}

static void handleApiWifiConfig() {
    if (!server.hasArg("ssid") || !server.hasArg("password") || !activeCfg) {
        server.send(400, "application/json", "{\"error\":\"Missing ssid or password\"}");
        return;
    }
    String newSsid = server.arg("ssid");
    String newPass = server.arg("password");
    newSsid.trim();
    if (newSsid.length() < 3 || newSsid.length() > 31 || newPass.length() < 8 || newPass.length() > 63) {
        server.send(400, "application/json", "{\"error\":\"SSID (3-31 chars) and Password (8-63 chars) required\"}");
        return;
    }
    strlcpy(activeCfg->ssid, newSsid.c_str(), sizeof(activeCfg->ssid));
    strlcpy(activeCfg->password, newPass.c_str(), sizeof(activeCfg->password));
    saveSystemConfig(*activeCfg);
    server.send(200, "application/json", "{\"status\":\"saved\",\"note\":\"Restart ESP8266 to apply\"}");
}

static void handleApiRestart() {
    server.send(200, "application/json", "{\"status\":\"restarting\"}");
    delay(200);
    ESP.restart();
}

void initHttpServer(SystemConfig &cfg) {
    activeCfg = &cfg;

    // Serve Main Launcher Interface directly from Boot Flash (PROGMEM) so it is NEVER missing
    server.on("/", HTTP_GET, []() {
        sendEmbeddedGzip("text/html", EMBED_INDEX_HTML_GZ, EMBED_INDEX_HTML_GZ_LEN);
    });
    server.on("/style.css", HTTP_GET, []() {
        sendEmbeddedGzip("text/css", EMBED_STYLE_CSS_GZ, EMBED_STYLE_CSS_GZ_LEN);
    });
    server.on("/app.js", HTTP_GET, []() {
        sendEmbeddedGzip("application/javascript", EMBED_APP_JS_GZ, EMBED_APP_JS_GZ_LEN);
    });
    server.on("/runtime/fridge.js", HTTP_GET, []() {
        sendEmbeddedGzip("application/javascript", EMBED_FRIDGE_JS_GZ, EMBED_FRIDGE_JS_GZ_LEN);
    });

    // Package & System APIs
    server.on("/api/status", HTTP_GET, handleApiStatus);
    server.on("/api/apps", HTTP_GET, handleApiApps);
    server.on("/api/app/icon", HTTP_GET, handleApiAppIcon);
    server.on("/api/app/file", HTTP_GET, handleApiAppFile);
    server.on("/api/app/files", HTTP_GET, handleApiAppBundleList);
    server.on("/api/install", HTTP_POST, handleUzipUploadComplete, handleUzipUploadData);
    server.on("/api/uninstall", HTTP_POST, handleApiUninstall);
    server.on("/api/config/wifi", HTTP_POST, handleApiWifiConfig);
    server.on("/api/restart", HTTP_POST, handleApiRestart);

    // Automatic OS Captive Portal Triggers
    server.on("/generate_204", HTTP_GET, handleCaptiveRedirect);
    server.on("/gen_204", HTTP_GET, handleCaptiveRedirect);
    server.on("/hotspot-detect.html", HTTP_GET, handleCaptiveRedirect);
    server.on("/library/test/success.html", HTTP_GET, handleCaptiveRedirect);
    server.on("/ncsi.txt", HTTP_GET, handleCaptiveRedirect);
    server.on("/connecttest.txt", HTTP_GET, handleCaptiveRedirect);
    server.on("/redirect", HTTP_GET, handleCaptiveRedirect);
    server.on("/success.txt", HTTP_GET, handleCaptiveRedirect);
    server.onNotFound(handleCaptiveRedirect);

    httpUpdater.setup(&server, "/api/ota");
    server.begin();
}

void processHttpServer() {
    server.handleClient();
}
