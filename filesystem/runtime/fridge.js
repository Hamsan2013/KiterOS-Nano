(function(root) {
  function initFridgeWrapperRuntime() {
    const VERSION = "2.0.0";

    function parseFrgDescriptor(frgText) {
      const descriptor = {
        package: "com.kiteros.unnamed",
        entrypoint: "index.html",
        orientation: "portrait",
        fullscreen: true,
        offline_ready: true
      };

      if (!frgText || typeof frgText !== "string") return descriptor;

      const lines = frgText.replace(/\r\n/g, "\n").split("\n");
      for (let i = 0; i < lines.length; i++) {
        const line = lines[i].trim();
        if (!line || line.startsWith("#")) continue;
        const m = line.match(/^([a-zA-Z_]\w*)\s+(?:"([^"]+)"|(true|false|\S+))$/);
        if (m) {
          const key = m[1].toLowerCase();
          const val = m[2] !== undefined ? m[2] : (m[3] === "true" ? true : (m[3] === "false" ? false : m[3]));
          descriptor[key] = val;
        }
      }
      return descriptor;
    }

    function prepareHtmlPayload(rawHtml, descriptor, manifest) {
      const bridgeScript = `<script>
window.Fridge = {
  version: "${VERSION}",
  package: ${JSON.stringify(descriptor.package)},
  appId: ${JSON.stringify((manifest && manifest.id) || "app")},
  saveData: function(key, val) {
    localStorage.setItem("frg_" + this.appId + "_" + key, JSON.stringify(val));
  },
  loadData: function(key, fallback) {
    const raw = localStorage.getItem("frg_" + this.appId + "_" + key);
    return raw !== null ? JSON.parse(raw) : fallback;
  }
};
<\/script>`;

      if (rawHtml.includes("</head>")) {
        return rawHtml.replace("</head>", bridgeScript + "</head>");
      }
      return bridgeScript + rawHtml;
    }

    return {
      VERSION,
      parseFrgDescriptor,
      prepareHtmlPayload,
      toString: () => initFridgeWrapperRuntime.toString()
    };
  }

  const runtime = initFridgeWrapperRuntime();
  if (typeof module !== "undefined" && module.exports) {
    module.exports = runtime;
  }
  root.FridgeRuntime = runtime;
})(typeof window !== "undefined" ? window : globalThis);
