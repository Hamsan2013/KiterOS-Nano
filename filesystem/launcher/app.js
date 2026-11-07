(function() {
  const OFFLINE_PREFIX = "kiter_pkg_";
  let boardOnline = false;

  const connBadge    = document.getElementById("conn-badge");
  const appsList     = document.getElementById("apps-list");
  const gamesList    = document.getElementById("games-list");
  const sysMetrics   = document.getElementById("sys-metrics");
  const runnerModal  = document.getElementById("runner-modal");
  const runnerTitle  = document.getElementById("runner-title");
  const runnerMeta   = document.getElementById("runner-meta");
  const appIframe    = document.getElementById("app-iframe");

  // 1. Tab Switching
  document.querySelectorAll(".tab").forEach(btn => {
    btn.addEventListener("click", () => {
      document.querySelectorAll(".tab").forEach(b => b.classList.remove("active"));
      document.querySelectorAll(".panel").forEach(p => p.classList.remove("active"));
      btn.classList.add("active");
      document.getElementById("tab-" + btn.dataset.tab).classList.add("active");
    });
  });

  // 2. Standard PKZIP / .UZIP Store-Mode Builder in Pure JavaScript
  function crc32Bytes(uint8Arr) {
    let crc = 0xFFFFFFFF;
    for (let i = 0; i < uint8Arr.length; i++) {
      crc ^= uint8Arr[i];
      for (let k = 0; k < 8; k++) {
        crc = (crc >>> 1) ^ (0xEDB88320 & (-(crc & 1)));
      }
    }
    return (crc ^ 0xFFFFFFFF) >>> 0;
  }

  function createUzipBlobFromFiles(fileMap) {
    const enc = new TextEncoder();
    const localParts = [];
    const centralParts = [];
    let offset = 0;
    const names = Object.keys(fileMap);

    names.forEach(name => {
      const nameBytes = enc.encode(name);
      const dataBytes = enc.encode(fileMap[name]);
      const crc = crc32Bytes(dataBytes);
      const size = dataBytes.length;

      const lfh = new Uint8Array(30 + nameBytes.length);
      const lv = new DataView(lfh.buffer);
      lv.setUint32(0, 0x04034b50, true);
      lv.setUint16(4, 20, true);
      lv.setUint16(6, 0, true);
      lv.setUint16(8, 0, true); // Method 0 (ZIP Store)
      lv.setUint32(14, crc, true);
      lv.setUint32(18, size, true);
      lv.setUint32(22, size, true);
      lv.setUint16(26, nameBytes.length, true);
      lfh.set(nameBytes, 30);

      const cdh = new Uint8Array(46 + nameBytes.length);
      const cv = new DataView(cdh.buffer);
      cv.setUint32(0, 0x02014b50, true);
      cv.setUint16(4, 20, true);
      cv.setUint16(6, 20, true);
      cv.setUint16(10, 0, true);
      cv.setUint32(16, crc, true);
      cv.setUint32(20, size, true);
      cv.setUint32(24, size, true);
      cv.setUint16(28, nameBytes.length, true);
      cv.setUint32(42, offset, true);
      cdh.set(nameBytes, 46);

      localParts.push(lfh, dataBytes);
      centralParts.push(cdh);
      offset += lfh.length + size;
    });

    let cdSize = 0;
    centralParts.forEach(p => { cdSize += p.length; });

    const eocd = new Uint8Array(22);
    const ev = new DataView(eocd.buffer);
    ev.setUint32(0, 0x06054b50, true);
    ev.setUint16(8, names.length, true);
    ev.setUint16(10, names.length, true);
    ev.setUint32(12, cdSize, true);
    ev.setUint32(16, offset, true);

    return new Blob([...localParts, ...centralParts, eocd], { type: "application/zip" });
  }

  function buildUzipFromStudio() {
    const rawId = document.getElementById("build-id").value.trim().replace(/[^a-zA-Z0-9_-]/g, "");
    const id = rawId.length >= 2 ? rawId : "mygame";
    const name = document.getElementById("build-name").value.trim() || id;
    const type = document.getElementById("build-type").value;
    const frgText = document.getElementById("build-frg").value;
    const htmlText = document.getElementById("build-html").value;

    const desc = window.FridgeRuntime.parseFrgDescriptor(frgText);
    const entryHtml = desc.entrypoint || "index.html";

    const manifestObj = {
      id: id,
      name: name,
      version: "1.0.0",
      type: type,
      wrapper: "app.frg",
      entry: entryHtml
    };

    const files = {
      "manifest.json": JSON.stringify(manifestObj),
      "app.frg": frgText
    };
    files[entryHtml] = htmlText;

    return {
      id,
      manifest: manifestObj,
      descriptor: desc,
      htmlText,
      blob: createUzipBlobFromFiles(files)
    };
  }

  async function uploadZipFileToEsp(blobOrFile, filename) {
    const fd = new FormData();
    fd.append("package", blobOrFile, filename);
    const res = await fetch("/api/install", { method: "POST", body: fd });
    const data = await res.json();
    if (!res.ok) {
      throw new Error(data.error || "Upload failed");
    }
    return data.id;
  }

  // 3. Offline LocalStorage Helpers
  function getOfflinePackages() {
    const map = {};
    for (let i = 0; i < localStorage.length; i++) {
      const k = localStorage.key(i);
      if (k && k.startsWith(OFFLINE_PREFIX)) {
        try {
          const pkg = JSON.parse(localStorage.getItem(k));
          if (pkg && pkg.manifest && pkg.manifest.id) {
            map[pkg.manifest.id] = pkg;
          }
        } catch (e) {}
      }
    }
    return map;
  }

  // 4. Fetch Board Status & Installed Packages
  async function refreshStatusAndApps() {
    const offlineMap = getOfflinePackages();
    let remoteApps = [];

    try {
      const stRes = await fetch("/api/status", { cache: "no-store" });
      if (!stRes.ok) throw new Error();
      const st = await stRes.json();
      boardOnline = true;
      connBadge.textContent = "ESP8266 Online (" + st.ip + " | Heap: " + st.free_heap + " B)";
      connBadge.style.color = "#10b981";
      renderSystemMetrics(st);

      const appRes = await fetch("/api/apps", { cache: "no-store" });
      if (appRes.ok) {
        const data = await appRes.json();
        remoteApps = data.apps || [];
      }
    } catch (e) {
      boardOnline = false;
      connBadge.textContent = "Offline Mode (Running from Phone Storage)";
      connBadge.style.color = "#f59e0b";
    }

    const merged = {};
    remoteApps.forEach(m => {
      merged[m.id] = { manifest: m, onBoard: true, onPhone: !!offlineMap[m.id] };
    });
    Object.keys(offlineMap).forEach(id => {
      if (!merged[id]) {
        merged[id] = { manifest: offlineMap[id].manifest, onBoard: false, onPhone: true };
      }
    });

    renderPackages(Object.values(merged));
  }

  function renderSystemMetrics(st) {
    sysMetrics.textContent = "";
    const items = [
      ["OS / Version", st.os + " v" + st.version],
      ["Chip ID / CPU", st.chip_id + " @ " + st.cpu_mhz + " MHz"],
      ["Sketch / Free OTA", Math.round(st.sketch_bytes / 1024) + " KB / " + Math.round(st.ota_free_bytes / 1024) + " KB"],
      ["Free Heap / Frag", st.free_heap + " B (" + st.heap_frag + "%)"],
      ["LittleFS Storage", Math.round(st.fs_used / 1024) + " KB / " + Math.round(st.fs_total / 1024) + " KB"],
      ["Wi-Fi SoftAP", st.ssid + " (" + st.clients + " client(s))"]
    ];
    items.forEach(([k, v]) => {
      const card = document.createElement("div");
      card.className = "card";
      const h = document.createElement("h3");
      h.textContent = k;
      const p = document.createElement("p");
      p.textContent = v;
      card.appendChild(h);
      card.appendChild(p);
      sysMetrics.appendChild(card);
    });
  }

  function renderPackages(list) {
    appsList.textContent = "";
    gamesList.textContent = "";

    const apps = list.filter(x => x.manifest.type !== "game");
    const games = list.filter(x => x.manifest.type === "game");

    if (apps.length === 0) {
      const p = document.createElement("p");
      p.textContent = "No applications installed yet. Open '+ .ZIP Studio' to upload or create one.";
      appsList.appendChild(p);
    }
    if (games.length === 0) {
      const p = document.createElement("p");
      p.textContent = "No HTML5 games installed yet. Open '+ .ZIP Studio' to build and install a game.";
      gamesList.appendChild(p);
    }

    list.forEach(item => {
      const m = item.manifest;
      const card = document.createElement("div");
      card.className = "card";

      const title = document.createElement("h3");
      title.textContent = m.name + " (v" + m.version + ")";

      const status = document.createElement("p");
      status.textContent = item.onPhone
        ? "Status: Saved on Phone (Works Offline)"
        : "Status: Hosted on ESP8266";

      const actions = document.createElement("div");
      actions.className = "card-actions";

      const btnLaunch = document.createElement("button");
      btnLaunch.className = "btn primary";
      btnLaunch.textContent = "Play / Launch";
      btnLaunch.onclick = () => launchPackage(m.id);
      actions.appendChild(btnLaunch);

      if (item.onBoard && boardOnline) {
        const btnDl = document.createElement("button");
        btnDl.className = "btn";
        btnDl.textContent = item.onPhone ? "Sync Phone Copy" : "Save to Phone";
        btnDl.onclick = () => downloadPackageToPhone(m.id);
        actions.appendChild(btnDl);
      }

      if (item.onPhone) {
        const btnExport = document.createElement("button");
        btnExport.className = "btn";
        btnExport.textContent = "Export Offline .html";
        btnExport.onclick = () => exportStandaloneHtml(m.id);
        actions.appendChild(btnExport);
      }

      if (item.onBoard && boardOnline) {
        const btnDel = document.createElement("button");
        btnDel.className = "btn danger";
        btnDel.textContent = "Uninstall";
        btnDel.onclick = () => uninstallPackage(m.id);
        actions.appendChild(btnDel);
      }

      card.appendChild(title);
      card.appendChild(status);
      card.appendChild(actions);

      if (m.type === "game") {
        gamesList.appendChild(card);
      } else {
        appsList.appendChild(card);
      }
    });
  }

  // 5. Launch HTML Game via FRIDGE (app.frg) Wrapper
  async function launchPackage(appId) {
    let manifest = null;
    let frgText = "";
    let htmlText = "";

    const localRaw = localStorage.getItem(OFFLINE_PREFIX + appId);
    if (localRaw) {
      const bundle = JSON.parse(localRaw);
      manifest = bundle.manifest;
      frgText = bundle.files["app.frg"] || "";
      const desc = window.FridgeRuntime.parseFrgDescriptor(frgText);
      const entry = desc.entrypoint || manifest.entry || "index.html";
      htmlText = bundle.files[entry] || "<h3>Missing index.html in package</h3>";
      openHtmlRunnerModal(manifest, desc, htmlText, "Offline Phone Copy");
      return;
    }

    try {
      const mRes = await fetch("/api/app/file?id=" + encodeURIComponent(appId) + "&path=manifest.json");
      if (!mRes.ok) throw new Error("Cannot load manifest.json");
      manifest = await mRes.json();

      const fRes = await fetch("/api/app/file?id=" + encodeURIComponent(appId) + "&path=app.frg");
      if (fRes.ok) frgText = await fRes.text();

      const desc = window.FridgeRuntime.parseFrgDescriptor(frgText);
      const entry = desc.entrypoint || manifest.entry || "index.html";

      const hRes = await fetch("/api/app/file?id=" + encodeURIComponent(appId) + "&path=" + encodeURIComponent(entry));
      if (!hRes.ok) throw new Error("Cannot load HTML entry: " + entry);
      htmlText = await hRes.text();

      openHtmlRunnerModal(manifest, desc, htmlText, "ESP8266 Stream");
    } catch (e) {
      alert("Launch failed: " + e.message);
    }
  }

  function openHtmlRunnerModal(manifest, descriptor, rawHtml, sourceLabel) {
    runnerTitle.textContent = manifest.name;
    runnerMeta.textContent = "[" + descriptor.package + " | " + sourceLabel + "]";
    appIframe.srcdoc = window.FridgeRuntime.prepareHtmlPayload(rawHtml, descriptor, manifest);
    runnerModal.classList.remove("hidden");
  }

  // 6. Download Package to Phone & Export Standalone Offline HTML
  async function downloadPackageToPhone(appId) {
    try {
      const res = await fetch("/api/app/files?id=" + encodeURIComponent(appId));
      if (!res.ok) throw new Error("Cannot list package files");
      const info = await res.json();
      const filesMap = {};

      for (const relPath of info.files) {
        const fRes = await fetch("/api/app/file?id=" + encodeURIComponent(appId) + "&path=" + encodeURIComponent(relPath));
        if (!fRes.ok) throw new Error("Failed downloading " + relPath);
        filesMap[relPath] = await fRes.text();
      }

      const manifest = JSON.parse(filesMap["manifest.json"]);
      localStorage.setItem(OFFLINE_PREFIX + appId, JSON.stringify({
        manifest,
        files: filesMap,
        savedAt: Date.now()
      }));
      alert("Saved '" + manifest.name + "' to phone storage! You can now launch it or export a standalone offline HTML file.");
      refreshStatusAndApps();
    } catch (e) {
      alert("Download error: " + e.message);
    }
  }

  function exportStandaloneHtml(appId) {
    const raw = localStorage.getItem(OFFLINE_PREFIX + appId);
    if (!raw) return;
    const bundle = JSON.parse(raw);
    const desc = window.FridgeRuntime.parseFrgDescriptor(bundle.files["app.frg"] || "");
    const entry = desc.entrypoint || bundle.manifest.entry || "index.html";
    const fullHtml = window.FridgeRuntime.prepareHtmlPayload(bundle.files[entry] || "", desc, bundle.manifest);

    const blob = new Blob([fullHtml], { type: "text/html" });
    const a = document.createElement("a");
    a.href = URL.createObjectURL(blob);
    a.download = appId + "-offline.html";
    a.click();
  }

  async function uninstallPackage(appId) {
    if (!confirm("Uninstall '" + appId + "' from ESP8266?")) return;
    await fetch("/api/uninstall?id=" + encodeURIComponent(appId), { method: "POST" });
    refreshStatusAndApps();
  }

  // 7. Event Listeners for Studio & Uploaders
  document.getElementById("quick-zip-input").addEventListener("change", async (e) => {
    const file = e.target.files[0];
    if (!file) return;
    try {
      const id = await uploadZipFileToEsp(file, file.name);
      alert("Successfully installed package: " + id);
      refreshStatusAndApps();
    } catch (err) {
      alert("Install failed: " + err.message);
    }
    e.target.value = "";
  });

  document.getElementById("btn-upload-zip").addEventListener("click", async () => {
    const inp = document.getElementById("direct-zip-file");
    if (!inp.files.length) {
      alert("Please select a .zip or .uzip file first.");
      return;
    }
    try {
      const id = await uploadZipFileToEsp(inp.files[0], inp.files[0].name);
      alert("Installed package: " + id);
      inp.value = "";
      refreshStatusAndApps();
    } catch (err) {
      alert("Upload error: " + err.message);
    }
  });

  document.getElementById("btn-create-install").addEventListener("click", async () => {
    try {
      const pkg = buildUzipFromStudio();
      const id = await uploadZipFileToEsp(pkg.blob, pkg.id + ".uzip");
      alert("Built '" + id + ".uzip' in browser and installed to ESP8266!");
      refreshStatusAndApps();
    } catch (err) {
      alert("Build/Install failed: " + err.message);
    }
  });

  document.getElementById("btn-download-uzip").addEventListener("click", () => {
    const pkg = buildUzipFromStudio();
    const a = document.createElement("a");
    a.href = URL.createObjectURL(pkg.blob);
    a.download = pkg.id + ".uzip";
    a.click();
  });

  document.getElementById("btn-preview-game").addEventListener("click", () => {
    const pkg = buildUzipFromStudio();
    openHtmlRunnerModal(pkg.manifest, pkg.descriptor, pkg.htmlText, "Studio Live Preview");
  });

  document.getElementById("btn-close-runner").addEventListener("click", () => {
    appIframe.srcdoc = "";
    runnerModal.classList.add("hidden");
  });

  document.getElementById("btn-save-wifi").addEventListener("click", async () => {
    const ssid = document.getElementById("cfg-ssid").value.trim();
    const pass = document.getElementById("cfg-pass").value;
    const body = "ssid=" + encodeURIComponent(ssid) + "&password=" + encodeURIComponent(pass);
    const res = await fetch("/api/config/wifi", {
      method: "POST",
      headers: { "Content-Type": "application/x-www-form-urlencoded" },
      body
    });
    const data = await res.json();
    alert(data.status ? (data.status + " - " + data.note) : data.error);
  });

  document.getElementById("btn-restart").addEventListener("click", async () => {
    if (!confirm("Restart ESP8266 now?")) return;
    await fetch("/api/restart", { method: "POST" });
  });

  refreshStatusAndApps();
  setInterval(refreshStatusAndApps, 8000);
})();
