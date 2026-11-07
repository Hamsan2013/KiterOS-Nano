#include "http_server.h"
#include "sys_info.h"
#include "storage.h"
#include "uzip_manager.h"
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <LittleFS.h>

static ESP8266WebServer server(80);
static ESP8266HTTPUpdateServer httpUpdater;
static SystemConfig *activeCfg = nullptr;
static File uploadFile;
static bool uploadError = false;
static size_t uploadedBytes = 0;

static const char* getMimeType(const char *path) {
    if (strstr(path, ".html")) return "text/html";
    if (strstr(path, ".css"))  return "text/css";
    if (strstr(path, ".js"))   return "application/javascript";
    if (strstr(path, ".json")) return "application/json";
    if (strstr(path, ".frg"))  return "text/plain";
    if (strstr(path, ".png"))  return "image/png";
    return "application/octet-stream";
}

static bool serveStaticFile(const char *fsPath) {
    char gzPath[MAX_PATH_LEN];
    snprintf(gzPath, sizeof(gzPath), "%s.gz", fsPath);
    if (LittleFS.exists(gzPath)) {
        File f = LittleFS.open(gzPath, "r");
        if (f) {
            server.sendHeader("Cache-Control", "max-age=300");
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

static void handleCaptiveRedirect() {
    server.sendHeader("Location", "http://192.168.4.1/", true);
    server.send(302, "text/plain", "Redirecting to KiterOS Nano (http://192.168.4.1)");
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

// Streams an individual file from an installed app package:
// GET /api/app/file?id=<appId>&path=<relativePath>
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
    if (!serveStaticFile(fullPath)) {
        server.send(404, "application/json", "{\"error\":\"File not found\"}");
    }
}

// Lists all relative files inside /apps/<id> so the phone can download the entire app offline
static void appendDirFilesJson(const char *baseAppDir, const char *subDir, char *out, size_t maxLen, size_t &pos, bool &first) {
    char curDir[MAX_PATH_LEN];
    if (subDir[0] == '\0') {
        strlcpy(curDir, baseAppDir, sizeof(curDir));
    } else {
        snprintf(curDir, sizeof(curDir), "%s/%s", baseAppDir, subDir);
    }

    Dir d = LittleFS.openDir(curDir);
    while (d.next()) {
        char relPath[MAX_PATH_LEN];
        if (subDir[0] == '\0') {
            strlcpy(relPath, d.fileName().c_str(), sizeof(relPath));
        } else {
            snprintf(relPath, sizeof(relPath), "%s/%s", subDir, d.fileName().c_str());
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
    if (uploadError || uploadedBytes == 0) {
        LittleFS.remove("/tmp/upload.uzip");
        server.send(400, "application/json", "{\"error\":\"Upload failed or exceeded 128 KB limit\"}");
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

    char okBuf[96];
    snprintf(okBuf, sizeof(okBuf), "{\"status\":\"installed\",\"id\":\"%s\"}", appId);
    server.send(200, "application/json", okBuf);
}

static void handleApiUninstall() {
    if (!server.hasArg("id")) {
        server.send(400, "application/json", "{\"error\":\"Missing id\"}");
        return;
    }
    String id = server.arg("id");
    if (!uninstallAppPackage(id.c_str())) {
        server.send(400, "application/json", "{\"error\":\"Failed to uninstall package\"}");
        return;
    }
    server.send(200, "application/json", "{\"status\":\"uninstalled\"}");
}

static void handleApiWifiConfig() {
    if (!server.hasArg("ssid") || !server.hasArg("password") || !activeCfg) {
        server.send(400, "application/json", "{\"error\":\"Provide ssid and password (>=8 chars)\"}");
        return;
    }
    String newSsid = server.arg("ssid");
    String newPass = server.arg("password");
    newSsid.trim();
    if (newSsid.length() < 3 || newSsid.length() > 31 || newPass.length() < 8 || newPass.length() > 63) {
        server.send(400, "application/json", "{\"error\":\"Invalid SSID (3-31 chars) or password (8-63 chars)\"}");
        return;
    }
    strlcpy(activeCfg->ssid, newSsid.c_str(), sizeof(activeCfg->ssid));
    strlcpy(activeCfg->password, newPass.c_str(), sizeof(activeCfg->password));
    if (!saveSystemConfig(*activeCfg)) {
        server.send(500, "application/json", "{\"error\":\"Failed to save config\"}");
        return;
    }
    server.send(200, "application/json", "{\"status\":\"saved\",\"note\":\"Restart board to apply new Wi-Fi settings\"}");
}

static void handleApiRestart() {
    server.send(200, "application/json", "{\"status\":\"restarting\"}");
    delay(200);
    ESP.restart();
}

void initHttpServer(SystemConfig &cfg) {
    activeCfg = &cfg;

    server.on("/", HTTP_GET, []() {
        if (!serveStaticFile("/launcher/index.html")) {
            server.send(500, "text/plain", "KiterOS Nano: Missing /launcher/index.html on LittleFS");
        }
    });
    server.on("/style.css", HTTP_GET, []() {
        if (!serveStaticFile("/launcher/style.css")) server.send(404, "text/plain", "Not found");
    });
    server.on("/app.js", HTTP_GET, []() {
        if (!serveStaticFile("/launcher/app.js")) server.send(404, "text/plain", "Not found");
    });
    server.on("/runtime/fridge.js", HTTP_GET, []() {
        if (!serveStaticFile("/runtime/fridge.js")) server.send(404, "text/plain", "Not found");
    });

    // API endpoints
    server.on("/api/status", HTTP_GET, handleApiStatus);
    server.on("/api/apps", HTTP_GET, handleApiApps);
    server.on("/api/app/file", HTTP_GET, handleApiAppFile);
    server.on("/api/app/files", HTTP_GET, handleApiAppBundleList);
    server.on("/api/install", HTTP_POST, handleUzipUploadComplete, handleUzipUploadData);
    server.on("/api/uninstall", HTTP_POST, handleApiUninstall);
    server.on("/api/config/wifi", HTTP_POST, handleApiWifiConfig);
    server.on("/api/restart", HTTP_POST, handleApiRestart);

    // Captive Portal OS probes
    server.on("/generate_204", HTTP_GET, handleCaptiveRedirect);
    server.on("/gen_204", HTTP_GET, handleCaptiveRedirect);
    server.on("/hotspot-detect.html", HTTP_GET, handleCaptiveRedirect);
    server.on("/ncsi.txt", HTTP_GET, handleCaptiveRedirect);
    server.on("/connecttest.txt", HTTP_GET, handleCaptiveRedirect);
    server.on("/redirect", HTTP_GET, handleCaptiveRedirect);
    server.onNotFound(handleCaptiveRedirect);

    // Safe HTTP OTA endpoint (/api/ota)
    httpUpdater.setup(&server, "/api/ota");

    server.begin();
}

void processHttpServer() {
    server.handleClient();
}
