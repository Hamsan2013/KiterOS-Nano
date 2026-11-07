(function() {
  const OFFLINE_PREFIX = "kiter_pkg_";
  const MAX_UZIP_BYTES = 1024; // Strict 1 KB UZIP rule
  let boardOnline = false;

  const connBadge   = document.getElementById("conn-badge");
  const gamesList   = document.getElementById("games-list");
  const appsList    = document.getElementById("apps-list");
  const sysMetrics  = document.getElementById("sys-metrics");
  const runnerModal = document.getElementById("runner-modal");
  const runnerTitle = document.getElementById("runner-title");
  const runnerIcon  = document.getElementById("runner-icon");
  const appIframe   = document.getElementById("app-iframe");
  const filePicker  = document.getElementById("uzip-file-picker");
  const fileInfo    = document.getElementById("file-info");

  document.querySelectorAll(".tab").forEach(btn => {
    btn.addEventListener("click", () => {
      document.querySelectorAll(".tab").forEach(b => b.classList.remove("active"));
      document.querySelectorAll(".panel").forEach(p => p.classList.remove("active"));
      btn.classList.add("active");
      document.getElementById("tab-" + btn.dataset.tab).classList.add("active");
    });
  });

  function getOfflinePackages() {
    const map = {};
    for (let i = 0; i < localStorage.length; i++) {
      const k = localStorage.key(i);
      if (k && k.startsWith(OFFLINE_PREFIX)) {
        try {
          const pkg = JSON.parse(localStorage.getItem(k));
          if (pkg && pkg.manifest && pkg.manifest.id) map[pkg.manifest.id] = pkg;
        } catch (e) {}
      }
    }
    return map;
  }

  async function refreshAll() {
    const offlineMap = getOfflinePackages();
    let remoteApps = [];

    try {
      const stRes = await fetch("/api/status", { cache: "no-store" });
      if (!stRes.ok) throw new Error();
      const st = await stRes.json();
      boardOnline = true;
      connBadge.textContent = "Online (" + st.ip + " | Heap: " + st.free_heap + " B)";
      connBadge.style.color = "#10b981";
      renderSystem(st);

      const appRes = await fetch("/api/apps", { cache: "no-store" });
      if (appRes.ok) {
        const data = await appRes.json();
        remoteApps = data.apps || [];
      }
    } catch (e) {
      boardOnline = false;
      connBadge.textContent = "Offline Mode (Phone Storage)";
      connBadge.style.color = "#f59e0b";
    }

    const merged = {};
    remoteApps.forEach(m => {
      merged[m.id] = { manifest: m, onBoard: true, onPhone: !!offlineMap[m.id], offlinePkg: offlineMap[m.id] };
    });
    Object.keys(offlineMap).forEach(id => {
      if (!merged[id]) {
        merged[id] = { manifest: offlineMap[id].manifest, onBoard: false, onPhone: true, offlinePkg: offlineMap[id] };
      }
    });

    renderGrid(Object.values(merged));
  }

  function renderSystem(st) {
    sysMetrics.textContent = "";
    [
      ["OS Version", st.os + " v" + st.version],
      ["Chip / CPU", st.chip_id + " @ " + st.cpu_mhz + " MHz"],
      ["Free Heap", st.free_heap + " B (Frag: " + st.heap_frag + "%)"],
      ["LittleFS Storage", Math.round(st.fs_used / 1024) + " KB / " + Math.round(st.fs_total / 1024) + " KB"]
    ].forEach(([k, v]) => {
      const c = document.createElement("div");
      c.className = "card";
      const h = document.createElement("h3"); h.textContent = k;
      const p = document.createElement("p"); p.textContent = v;
      c.appendChild(h); c.appendChild(p);
      sysMetrics.appendChild(c);
    });
  }

  function renderGrid(items) {
    gamesList.textContent = "";
    appsList.textContent = "";

    const games = items.filter(x => x.manifest.type === "game");
    const apps  = items.filter(x => x.manifest.type !== "game");

    if (games.length === 0) {
      const p = document.createElement("p");
      p.textContent = "No games installed. Tap '+ Select .uzip' to install a 1 KB .uzip game.";
      gamesList.appendChild(p);
    }
    if (apps.length === 0) {
      const p = document.createElement("p");
      p.textContent = "No applications installed.";
      appsList.appendChild(p);
    }

    items.forEach(item => {
      const m = item.manifest;
      const tile = document.createElement("div");
      tile.className = "app-tile";

      const img = document.createElement("img");
      img.className = "app-icon";
      img.alt = m.name;
      if (item.offlinePkg && item.offlinePkg.iconDataUrl) {
        img.src = item.offlinePkg.iconDataUrl;
      } else {
        img.src = "/api/app/icon?id=" + encodeURIComponent(m.id) + "&t=" + Date.now();
      }

      const title = document.createElement("h3");
      title.textContent = m.name;

      const sub = document.createElement("small");
      sub.textContent = item.onPhone ? "v" + m.version + " • Offline Ready" : "v" + m.version;

      const actions = document.createElement("div");
      actions.className = "tile-actions";

      const btnPlay = document.createElement("button");
      btnPlay.className = "btn primary";
      btnPlay.textContent = "Open";
      btnPlay.onclick = () => launchApp(m.id, img.src);
      actions.appendChild(btnPlay);

      if (item.onBoard && boardOnline) {
        const btnCache = document.createElement("button");
        btnCache.className = "btn";
        btnCache.textContent = item.onPhone ? "Synced" : "Save";
        btnCache.onclick = () => cacheAppToPhone(m.id);
        actions.appendChild(btnCache);

        const btnDel = document.createElement("button");
        btnDel.className = "btn danger";
        btnDel.textContent = "×";
        btnDel.onclick = () => uninstallApp(m.id);
        actions.appendChild(btnDel);
      }

      tile.appendChild(img);
      tile.appendChild(title);
      tile.appendChild(sub);
      tile.appendChild(actions);

      if (m.type === "game") gamesList.appendChild(tile);
      else appsList.appendChild(tile);
    });
  }

  async function sendUzipToEsp(file) {
    if (!file) return;
    if (file.size > MAX_UZIP_BYTES) {
      alert("Rejected! '" + file.name + "' is " + file.size + " bytes. UZIP rule requires <= 1024 bytes (1 KB).");
      return;
    }
    const fd = new FormData();
    fd.append("package", file, file.name);
    const res = await fetch("/api/install", { method: "POST", body: fd });
    const data = await res.json();
    if (!res.ok) {
      alert("ESP8266 Error: " + (data.error || "Install failed"));
      return;
    }
    await cacheAppToPhone(data.id);
    alert("Installed & Uncompressed '" + data.id + "' (" + data.bytes + " B)!");
    refreshAll();
  }

  async function blobToDataURL(blob) {
    return new Promise(resolve => {
      const r = new FileReader();
      r.onloadend = () => resolve(r.result);
      r.readAsDataURL(blob);
    });
  }

  // Downloads all uncompressed files + PNG icon from ESP8266 into phone storage
  async function cacheAppToPhone(appId) {
    try {
      const listRes = await fetch("/api/app/files?id=" + encodeURIComponent(appId));
      if (!listRes.ok) return;
      const info = await listRes.json();
      const filesMap = {};

      for (const relPath of info.files) {
        if (relPath.endsWith(".png")) continue;
        const fRes = await fetch("/api/app/file?id=" + encodeURIComponent(appId) + "&path=" + encodeURIComponent(relPath));
        if (fRes.ok) filesMap[relPath] = await fRes.text();
      }

      let iconDataUrl = "";
      const iconRes = await fetch("/api/app/icon?id=" + encodeURIComponent(appId));
      if (iconRes.ok) {
        iconDataUrl = await blobToDataURL(await iconRes.blob());
      }

      const manifest = JSON.parse(filesMap["manifest.json"]);
      localStorage.setItem(OFFLINE_PREFIX + appId, JSON.stringify({
        manifest,
        files: filesMap,
        iconDataUrl,
        savedAt: Date.now()
      }));
    } catch (e) {}
  }

  async function launchApp(appId, iconSrc) {
    let manifest, frgText = "", htmlText = "";
    const localRaw = localStorage.getItem(OFFLINE_PREFIX + appId);

    if (localRaw) {
      const pkg = JSON.parse(localRaw);
      manifest = pkg.manifest;
      frgText = pkg.files["app.frg"] || "";
      const desc = window.FridgeRuntime.parseFrgDescriptor(frgText);
      const entry = desc.entrypoint || manifest.entry || "index.html";
      htmlText = pkg.files[entry] || "";
      showRunner(manifest, desc, htmlText, pkg.iconDataUrl || iconSrc);
      return;
    }

    try {
      const mRes = await fetch("/api/app/file?id=" + encodeURIComponent(appId) + "&path=manifest.json");
      manifest = await mRes.json();
      const fRes = await fetch("/api/app/file?id=" + encodeURIComponent(appId) + "&path=app.frg");
      if (fRes.ok) frgText = await fRes.text();
      const desc = window.FridgeRuntime.parseFrgDescriptor(frgText);
      const entry = desc.entrypoint || manifest.entry || "index.html";
      const hRes = await fetch("/api/app/file?id=" + encodeURIComponent(appId) + "&path=" + encodeURIComponent(entry));
      htmlText = await hRes.text();
      showRunner(manifest, desc, htmlText, iconSrc);
    } catch (e) {
      alert("Failed to launch app: " + e.message);
    }
  }

  function showRunner(manifest, desc, rawHtml, iconSrc) {
    runnerTitle.textContent = manifest.name;
    runnerIcon.src = iconSrc || "";
    appIframe.srcdoc = window.FridgeRuntime.prepareHtmlPayload(rawHtml, desc, manifest);
    runnerModal.classList.remove("hidden");
  }

  async function uninstallApp(appId) {
    if (!confirm("Uninstall '" + appId + "'?")) return;
    localStorage.removeItem(OFFLINE_PREFIX + appId);
    if (boardOnline) {
      await fetch("/api/uninstall?id=" + encodeURIComponent(appId), { method: "POST" });
    }
    refreshAll();
  }

  document.getElementById("quick-uzip").addEventListener("change", e => {
    if (e.target.files[0]) sendUzipToEsp(e.target.files[0]);
    e.target.value = "";
  });

  filePicker.addEventListener("change", e => {
    const f = e.target.files[0];
    if (!f) { fileInfo.textContent = "No .uzip file selected"; return; }
    fileInfo.textContent = f.name + " (" + f.size + " bytes / max 1024 bytes)";
    fileInfo.style.color = f.size <= MAX_UZIP_BYTES ? "#00e5ff" : "#ef4444";
  });

  document.getElementById("btn-install-uzip").addEventListener("click", () => {
    if (!filePicker.files[0]) { alert("Select a .uzip file first."); return; }
    sendUzipToEsp(filePicker.files[0]);
  });

  document.getElementById("btn-close-runner").addEventListener("click", () => {
    appIframe.srcdoc = "";
    runnerModal.classList.add("hidden");
  });

  document.getElementById("btn-save-wifi").addEventListener("click", async () => {
    const s = document.getElementById("cfg-ssid").value.trim();
    const p = document.getElementById("cfg-pass").value;
    const res = await fetch("/api/config/wifi", {
      method: "POST",
      headers: { "Content-Type": "application/x-www-form-urlencoded" },
      body: "ssid=" + encodeURIComponent(s) + "&password=" + encodeURIComponent(p)
    });
    const d = await res.json();
    alert(d.status || d.error);
  });

  document.getElementById("btn-restart").addEventListener("click", async () => {
    if (confirm("Restart ESP8266?")) await fetch("/api/restart", { method: "POST" });
  });

  refreshAll();
  setInterval(refreshAll, 8000);
})();
