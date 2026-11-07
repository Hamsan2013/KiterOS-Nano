(function() {
  const OFFLINE_PREFIX = "kiter_pkg_";
  let currentVm = null;
  let boardOnline = false;

  const connBadge = document.getElementById("conn-badge");
  const appsList = document.getElementById("apps-list");
  const gamesList = document.getElementById("games-list");
  const sysMetrics = document.getElementById("sys-metrics");
  const runnerModal = document.getElementById("runner-modal");
  const runnerTitle = document.getElementById("runner-title");
  const fridgeUi = document.getElementById("fridge-ui");
  const fridgeCanvas = document.getElementById("fridge-canvas");
  const fridgeConsole = document.getElementById("fridge-console");

  // Tab navigation
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
          if (pkg && pkg.manifest && pkg.manifest.id) {
            map[pkg.manifest.id] = pkg;
          }
        } catch (e) {}
      }
    }
    return map;
  }

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
      connBadge.textContent = "Offline Mode (Phone Local Storage)";
      connBadge.style.color = "#f59e0b";
    }

    const merged = {};
    remoteApps.forEach(m => { merged[m.id] = { manifest: m, onBoard: true, onPhone: !!offlineMap[m.id] }; });
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
      ["LittleFS Used", Math.round(st.fs_used / 1024) + " KB / " + Math.round(st.fs_total / 1024) + " KB"],
      ["Hotspot / Clients", st.ssid + " (" + st.clients + " connected)"]
    ];
    items.forEach(([k, v]) => {
      const div = document.createElement("div");
      div.className = "card";
      const h = document.createElement("h3");
      h.textContent = k;
      const p = document.createElement("p");
      p.textContent = v;
      div.appendChild(h);
      div.appendChild(p);
      sysMetrics.appendChild(div);
    });
  }

  function renderPackages(list) {
    appsList.textContent = "";
    gamesList.textContent = "";

    if (list.length === 0) {
      const empty = document.createElement("p");
      empty.textContent = "No packages installed yet. Upload a .uzip package or use FRIDGE Dev.";
      appsList.appendChild(empty);
      return;
    }

    list.forEach(item => {
      const m = item.manifest;
      const card = document.createElement("div");
      card.className = "card";

      const title = document.createElement("h3");
      title.textContent = m.name + " (v" + m.version + ")";

      const badge = document.createElement("p");
      badge.textContent = (item.onPhone ? "[Saved on Phone - Offline Ready] " : "[Requires ESP8266] ") + (m.description || "");

      const actions = document.createElement("div");
      actions.className = "card-actions";

      const btnRun = document.createElement("button");
      btnRun.className = "btn primary";
      btnRun.textContent = "Launch";
      btnRun.onclick = () => launchPackage(m.id);
      actions.appendChild(btnRun);

      if (item.onBoard) {
        const btnSave = document.createElement("button");
        btnSave.className = "btn";
        btnSave.textContent = item.onPhone ? "Update Phone Copy" : "Download to Phone";
        btnSave.onclick = () => downloadPackageToPhone(m.id);
        actions.appendChild(btnSave);
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
      card.appendChild(badge);
      card.appendChild(actions);

      if (m.type === "game") {
        gamesList.appendChild(card);
      } else {
        appsList.appendChild(card);
      }
    });
  }

  async function downloadPackageToPhone(appId) {
    try {
      const res = await fetch("/api/app/files?id=" + encodeURIComponent(appId));
      if (!res.ok) throw new Error("Could not list package files");
      const info = await res.json();
      const filesMap = {};

      for (const relPath of info.files) {
        const fRes = await fetch("/api/app/file?id=" + encodeURIComponent(appId) + "&path=" + encodeURIComponent(relPath));
        if (!fRes.ok) throw new Error("Failed downloading " + relPath);
        filesMap[relPath] = await fRes.text();
      }

      const manifest = JSON.parse(filesMap["manifest.json"]);
      const bundle = { manifest, files: filesMap, savedAt: Date.now() };
      localStorage.setItem(OFFLINE_PREFIX + appId, JSON.stringify(bundle));
      alert("Package '" + manifest.name + "' saved to phone localStorage for offline use!");
      refreshStatusAndApps();
    } catch (e) {
      alert("Download failed: " + e.message);
    }
  }

  function exportStandaloneHtml(appId) {
    const raw = localStorage.getItem(OFFLINE_PREFIX + appId);
    if (!raw) return;
    const bundle = JSON.parse(raw);
    const entryPath = bundle.manifest.entry || "source/main.frg";
    const sourceCode = bundle.files[entryPath] || "";
    const runtimeSrc = window.FridgeRuntime ? window.FridgeRuntime.toString() : "";

    const html = `<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>${bundle.manifest.name} (Offline KiterOS)</title><style>body{background:#070b19;color:#f1f5f9;font-family:sans-serif;display:flex;flex-direction:column;align-items:center;padding:12px}#ui{width:320px;margin-bottom:8px}canvas{background:#0b132b;border:1px solid #00e5ff;border-radius:8px}pre{width:320px;color:#a7f3d0;font-size:12px}</style></head><body><h2>${bundle.manifest.name}</h2><div id="ui"></div><canvas id="cv" width="320" height="360"></canvas><pre id="log"></pre><script>window.FridgeRuntime=(${runtimeSrc})();const src=${JSON.stringify(sourceCode)};const vm=window.FridgeRuntime.createVM({canvas:document.getElementById('cv'),uiRoot:document.getElementById('ui'),onPrint:m=>{document.getElementById('log').textContent+=m+'\\n';}});vm.run(src);<\/script></body></html>`;

    const blob = new Blob([html], { type: "text/html" });
    const a = document.createElement("a");
    a.href = URL.createObjectURL(blob);
    a.download = appId + "-offline.html";
    a.click();
  }

  async function launchPackage(appId) {
    let sourceCode = null;
    let title = appId;

    const localRaw = localStorage.getItem(OFFLINE_PREFIX + appId);
    if (localRaw) {
      const bundle = JSON.parse(localRaw);
      title = bundle.manifest.name;
      const entry = bundle.manifest.entry || "source/main.frg";
      sourceCode = bundle.files[entry];
    } else {
      const mRes = await fetch("/api/app/file?id=" + encodeURIComponent(appId) + "&path=manifest.json");
      if (!mRes.ok) { alert("Cannot load manifest.json"); return; }
      const manifest = await mRes.json();
      title = manifest.name;
      const entry = manifest.entry || "source/main.frg";
      const sRes = await fetch("/api/app/file?id=" + encodeURIComponent(appId) + "&path=" + encodeURIComponent(entry));
      if (!sRes.ok) { alert("Cannot load entry source " + entry); return; }
      sourceCode = await sRes.text();
    }

    startFridgeModal(title, sourceCode, appId);
  }

  function startFridgeModal(title, code, appId) {
    if (currentVm) currentVm.stop();
    runnerTitle.textContent = title;
    fridgeUi.textContent = "";
    fridgeConsole.textContent = "";
    runnerModal.classList.remove("hidden");

    try {
      currentVm = window.FridgeRuntime.createVM({
        appId: appId || "sandbox",
        canvas: fridgeCanvas,
        uiRoot: fridgeUi,
        onPrint: msg => { fridgeConsole.textContent += msg + "\n"; }
      });
      currentVm.run(code);
    } catch (err) {
      fridgeConsole.textContent += "[FRIDGE ERROR] " + err.message + "\n";
    }
  }

  async function uninstallPackage(appId) {
    if (!confirm("Uninstall '" + appId + "' from ESP8266 LittleFS?")) return;
    await fetch("/api/uninstall?id=" + encodeURIComponent(appId), { method: "POST" });
    refreshStatusAndApps();
  }

  document.getElementById("uzip-input").addEventListener("change", async (e) => {
    const file = e.target.files[0];
    if (!file) return;
    const fd = new FormData();
    fd.append("package", file);
    const res = await fetch("/api/install", { method: "POST", body: fd });
    const data = await res.json();
    if (!res.ok) {
      alert("Install error: " + (data.error || "Unknown error"));
    } else {
      alert("Installed package: " + data.id);
      refreshStatusAndApps();
    }
    e.target.value = "";
  });

  document.getElementById("btn-run-fridge").addEventListener("click", () => {
    const code = document.getElementById("fridge-editor").value;
    startFridgeModal("FRIDGE Live Sandbox", code, "sandbox");
  });

  document.getElementById("btn-close-runner").addEventListener("click", () => {
    if (currentVm) currentVm.stop();
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
    alert(data.status || data.error);
  });

  document.getElementById("btn-restart").addEventListener("click", async () => {
    if (!confirm("Restart ESP8266 now?")) return;
    await fetch("/api/restart", { method: "POST" });
  });

  refreshStatusAndApps();
  setInterval(refreshStatusAndApps, 8000);
})();
