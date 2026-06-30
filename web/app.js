/* Metis Code Analyzer Plus dashboard logic. Vanilla JS, no build step. 7-bit ASCII. */
(function () {
  "use strict";

  var GRADE_COLORS = { A: "#36c98d", B: "#7bd66a", C: "#e7c64b", D: "#ef9b4a", F: "#e0586a" };
  var charts = {};
  var state = {
    files: [], issues: [], logs: [],
    history: [],
    sevFilter: "all",
    sort: { key: "cyclomatic", dir: -1 },
    maxRows: 500,
    locale: "en-US",
    lastReport: null,
    fontScale: 100,
    grades: { a: 90, b: 80, c: 65, d: 50 },
    languages: []
  };

  var I18N = window.MC_I18N || { "en-US": {} };

  /* ---- i18n helpers ---- */
  function t(key) {
    var dict = I18N[state.locale] || I18N["en-US"] || {};
    if (dict[key] !== undefined) return dict[key];
    var en = I18N["en-US"] || {};
    return en[key] !== undefined ? en[key] : key;
  }

  function el(id) { return document.getElementById(id); }
  function fmt(n) { return (n || 0).toLocaleString(); }

  function setCookie(k, v) { document.cookie = k + "=" + encodeURIComponent(v) + "; Path=/; Max-Age=31536000; SameSite=Strict"; }
  function getCookie(k) {
    var m = document.cookie.match(new RegExp("(?:^|; )" + k + "=([^;]*)"));
    return m ? decodeURIComponent(m[1]) : "";
  }

  function gradeFor(s) {
    var g = state.grades;
    if (s >= g.a) return "A"; if (s >= g.b) return "B";
    if (s >= g.c) return "C"; if (s >= g.d) return "D"; return "F";
  }
  function colorForScore(s) { return GRADE_COLORS[gradeFor(s)]; }

  /* ---- HTTP API helper: attaches session cookie automatically ---- */
  function api(path, opts) {
    /* credentials:"include" ensures the session cookie is sent on every
     * request regardless of browser privacy settings, including Firefox's
     * Enhanced Tracking Protection mode on localhost. */
    var merged = Object.assign({ credentials: "include" }, opts || {});
    return fetch(path, merged).then(function (r) {
      if (r.status === 401) { showLogin(true); throw new Error("unauthorized"); }
      if (!r.ok) throw new Error(path + " -> " + r.status);
      var ct = r.headers.get("content-type") || "";
      return ct.indexOf("application/json") >= 0 ? r.json() : r.text();
    });
  }

  /* ---- i18n ---- */
  function applyI18n() {
    document.documentElement.lang = state.locale.split("-")[0];
    var nodes = document.querySelectorAll("[data-i18n]");
    Array.prototype.forEach.call(nodes, function (n) { n.textContent = t(n.getAttribute("data-i18n")); });
    var phs = document.querySelectorAll("[data-i18n-ph]");
    Array.prototype.forEach.call(phs, function (n) { n.setAttribute("placeholder", t(n.getAttribute("data-i18n-ph"))); });
    if (state.lastReport) { renderHealth(state.lastReport.health); renderStats(state.lastReport); }
    renderIssues(); renderFiles(); renderLogs(); renderLanguages();
  }

  function renderLanguages() {
    if (!state.languages.length) return;
    var names = state.languages.map(function (l) { return l.name; });
    el("langNote").textContent = t("supportedLanguages") + " (" +
      state.languages.length + "): " + names.join(", ");
  }

  function renderSecurity(s) {
    var box = el("secBadges");
    var tls = el("badgeTls");
    var aes = el("badgeAes");
    var pqc = el("badgePqc");
    if (!box || !aes || !pqc) return;

    /* TLS protocol badge - mirrors Metis Antique Marketplace Plus */
    if (tls) {
      var proto = (s && s.protocol) || (s && s.tls ? "TLS 1.3" : "");
      tls.textContent = proto || "TLS";
      if (s && s.tls) {
        tls.className = "sec-badge tls";
        tls.title = "Protocol: " + proto;
      } else {
        tls.className = "sec-badge off";
        tls.title = "No TLS on this connection";
      }
    }

    /* AES-256-GCM badge */
    aes.textContent = "AES-256-GCM";
    if (s && s.aes_256_gcm) {
      aes.className = "sec-badge aes";
      aes.title = "Negotiated cipher: " + (s.cipher || "AES-256-GCM");
    } else {
      aes.className = "sec-badge off";
      aes.title = (s && s.tls)
        ? "Negotiated cipher: " + (s.cipher || "unknown") + " (not AES-256-GCM)"
        : "No TLS on this connection";
    }

    /* Post-Quantum TLS badge */
    pqc.textContent = "Post-Quantum TLS";
    if (s && s.post_quantum) {
      pqc.className = "sec-badge pqc";
      pqc.title = "Key exchange: " + (s.group || "ML-KEM hybrid");
    } else {
      pqc.className = "sec-badge off";
      pqc.title = (s && s.tls)
        ? "Classical key exchange: " + (s.group || "unknown") + " (ML-KEM not negotiated)"
        : "No TLS on this connection";
    }

    /* Always reveal the badge bar regardless of TLS state */
    box.hidden = false;

    /* Populate TLS detail panel */
    function setText(id, v) { var e = el(id); if (e) e.textContent = v || "--"; }
    setText("tlsActive",  (s && s.tls) ? "Yes" : "No (plain HTTP)");
    setText("tlsProto",   (s && s.protocol) || ((s && s.tls) ? "TLS 1.3" : "--"));
    setText("tlsCipher",  (s && s.cipher)   || ((s && s.tls) ? "TLS_AES_256_GCM_SHA384" : "--"));
    setText("tlsKex",     (s && s.group)    || ((s && s.tls) ? "X25519" : "--"));
    setText("tlsPq",      (s && s.post_quantum) ? ("Yes - " + (s.group || "X25519MLKEM768")) : ((s && s.tls) ? "No (classical groups)" : "--"));
    setText("tlsAes",     (s && s.aes_256_gcm)  ? "Yes - AES-256-GCM" : ((s && s.tls) ? ("No - " + (s.cipher || "unknown")) : "--"));
    setText("tlsCn",      (s && s.cert_cn) || ((s && s.tls) ? "Metis Code Analyzer Plus" : "--"));
  }

  function populateLanguages(locales, def) {
    var sel = el("lang");
    sel.innerHTML = "";
    locales.forEach(function (code) {
      var o = document.createElement("option");
      o.value = code;
      o.textContent = (I18N[code] && I18N[code]._name) ? I18N[code]._name : code;
      sel.appendChild(o);
    });
    var saved = getCookie("mclang");
    state.locale = (saved && locales.indexOf(saved) >= 0) ? saved
                 : (locales.indexOf(def) >= 0 ? def : locales[0]);
    sel.value = state.locale;
    sel.addEventListener("change", function () {
      state.locale = sel.value; setCookie("mclang", state.locale); applyI18n();
    });
  }

  /* ---- rendering ---- */
  /* ---- Dashboard render functions ---- */
  function renderHealth(h) {
    var g = gradeFor(h.tqi);
    el("grade").textContent = g; el("grade").style.color = GRADE_COLORS[g];
    el("tqi").textContent = h.tqi.toFixed(1) + " / 100";
    var factors = [
      ["robustness", h.robustness], ["security", h.security],
      ["efficiency", h.efficiency], ["transferability", h.transferability],
      ["changeability", h.changeability],
      ["portability", h.portability !== undefined ? h.portability : 100]
    ];
    var box = el("factors"); box.innerHTML = "";
    factors.forEach(function (f) {
      var row = document.createElement("div"); row.className = "factor-row";
      var c = colorForScore(f[1]);
      row.innerHTML =
        '<span class="fname">' + t(f[0]) + '</span>' +
        '<span class="bar"><span style="width:' + f[1].toFixed(0) + '%;background:' + c + '"></span></span>' +
        '<span class="fval" style="color:' + c + '">' + f[1].toFixed(0) + '</span>';
      box.appendChild(row);
    });
  }

  function renderStats(rep) {
    var debt = rep.debt || {};
    var cards = [
      ["statFiles", fmt(rep.file_count)],
      ["statLoc", fmt(rep.code_lines)],
      ["statFns", fmt(rep.total_functions)],
      ["statCx", (rep.avg_complexity || 0).toFixed(1)],
      ["statDup", fmt(rep.duplicate_blocks)],
      ["statDebt", (debt.total_hours || 0).toFixed(1) + " h"]
    ];
    var s = el("stats"); s.innerHTML = "";
    cards.forEach(function (c) {
      var d = document.createElement("div"); d.className = "stat";
      d.innerHTML = '<div class="v">' + c[1] + '</div><div class="k">' + t(c[0]) + '</div>';
      s.appendChild(d);
    });
    el("rootNote").textContent =
      t("noteScanned") + ": " + (rep.scanned_root || ".") +
      "  |  " + t("noteCost") + ": $" + (debt.cost || 0).toFixed(0) +
      "  |  " + t("noteRatio") + ": " + ((debt.debt_ratio || 0) * 100).toFixed(1) + "%";
  }

  /* ---- Chart.js helpers (instance map prevents double-init) ---- */
  function cssVar(name, fallback) {
    var v = getComputedStyle(document.documentElement).getPropertyValue(name);
    return (v && v.trim()) || fallback;
  }

  function makeChart(id, config) {
    if (typeof Chart === "undefined") return;
    if (charts[id]) charts[id].destroy();
    charts[id] = new Chart(el(id).getContext("2d"), config);
  }

  function renderCharts(rep, issues, files) {
    if (typeof Chart === "undefined") return;
    Chart.defaults.color = cssVar("--muted", "#8da2c0");
    Chart.defaults.borderColor = cssVar("--line", "#243352");
    var sev = { critical: 0, major: 0, minor: 0, info: 0 };
    issues.forEach(function (i) { if (sev[i.severity] !== undefined) sev[i.severity]++; });
    makeChart("sevChart", {
      type: "bar",
      data: { labels: [t("sevCritical"), t("sevMajor"), t("sevMinor"), t("sevInfo")],
        datasets: [{ data: [sev.critical, sev.major, sev.minor, sev.info],
          backgroundColor: ["#e0586a", "#ef9b4a", "#e7c64b", "#8da2c0"] }] },
      options: { plugins: { legend: { display: false } }, maintainAspectRatio: false }
    });
    var langs = rep.lines_by_language || {};
    var lnames = Object.keys(langs);
    makeChart("langChart", {
      type: "doughnut",
      data: { labels: lnames, datasets: [{ data: lnames.map(function (k) { return langs[k]; }),
        backgroundColor: ["#3fd0c9","#36c98d","#7bd66a","#e7c64b","#ef9b4a","#e0586a","#8da2c0","#6f8fd6"] }] },
      options: { plugins: { legend: { position: "right" } }, maintainAspectRatio: false }
    });
    var top = files.slice().sort(function (a, b) { return b.cyclomatic - a.cyclomatic; }).slice(0, 10);
    makeChart("cxChart", {
      type: "bar",
      data: { labels: top.map(function (f) { return f.path.split("/").pop(); }),
        datasets: [{ data: top.map(function (f) { return f.cyclomatic; }), backgroundColor: "#3fd0c9" }] },
      options: { indexAxis: "y", plugins: { legend: { display: false } }, maintainAspectRatio: false }
    });
  }

  function renderIssues() {
    var tbody = el("issuesTable").querySelector("tbody");
    var rows = state.issues.filter(function (i) {
      return state.sevFilter === "all" || i.severity === state.sevFilter;
    });
    tbody.innerHTML = ""; el("issuesEmpty").hidden = rows.length > 0;
    rows.slice(0, state.maxRows).forEach(function (i) {
      var issueKey = encodeURIComponent(i.rule_id + ":" + i.file + ":" + i.line);
      var currentStatus = i.status || "open";
      var tr = document.createElement("tr");
      tr.className = "clickable";
      tr.title = t("clickPreview") || "Click to preview source";
      /* v2.3.0: added Status column for issue lifecycle */
      var statusColors = { open: "", accepted: "status-accepted", wontfix: "status-wontfix", false_positive: "status-fp" };
      var statusLabels = { open: "Open", accepted: "Accepted", wontfix: "Won't Fix", false_positive: "False Positive" };
      tr.innerHTML =
        '<td><span class="pill ' + esc(i.severity) + '">' + t("sev" + cap(i.severity)) + '</span></td>' +
        '<td class="path">' + esc(i.rule_id) + '</td>' +
        '<td>' + esc(i.title) + '</td>' +
        '<td class="path">' + esc(i.cwe || "-") + '</td>' +
        '<td class="path">' + esc(i.file) + '</td>' +
        '<td class="num">' + esc(String(i.line)) + '</td>' +
        '<td class="num">' + esc(String(i.remediation_minutes)) + '</td>' +
        '<td class="issue-status-cell"><select class="issue-status-sel ' + esc(statusColors[currentStatus] || "") + '">' +
          ['open','accepted','wontfix','false_positive'].map(function (s) {
            return '<option value="' + s + '"' + (s === currentStatus ? ' selected' : '') + '>' + statusLabels[s] + '</option>';
          }).join('') +
        '</select></td>';
      var sel = tr.querySelector(".issue-status-sel");
      sel.addEventListener("change", function (ev) {
        ev.stopPropagation();
        var newStatus = sel.value;
        sel.className = "issue-status-sel " + (statusColors[newStatus] || "");
        setIssueStatus(issueKey, newStatus, "", function () {
          i.status = newStatus;
        });
      });
      sel.addEventListener("click", function (ev) { ev.stopPropagation(); });
      tr.addEventListener("click", function () { showSourcePreview(i.file, i.line); });
      tbody.appendChild(tr);
    });
  }

  function showSourcePreview(path, line) {
    var pv = el("srcPreview");
    var lb = el("srcLabel");
    var tb = el("srcLines");
    lb.textContent = path + ":" + line;
    tb.innerHTML = '<tr><td colspan="2">Loading...</td></tr>';
    pv.hidden = false;
    pv.scrollIntoView({ behavior: "smooth", block: "nearest" });
    api("/api/source?path=" + encodeURIComponent(path) + "&line=" + line)
      .then(function (d) {
        tb.innerHTML = "";
        (d.lines || []).forEach(function (r) {
          var tr = document.createElement("tr");
          if (r.n === line) tr.className = "hl";
          var td1 = document.createElement("td");
          td1.className = "ln"; td1.textContent = String(r.n);
          var td2 = document.createElement("td"); td2.textContent = r.text;
          tr.appendChild(td1); tr.appendChild(td2);
          tb.appendChild(tr);
        });
      }).catch(function (e) {
        tb.innerHTML = '<tr><td colspan="2">Preview unavailable: ' + esc(e.message) + '</td></tr>';
      });
  }

  function renderFiles() {
    var tbody = el("filesTable").querySelector("tbody");
    var key = state.sort.key, dir = state.sort.dir;
    var rows = state.files.slice().sort(function (a, b) {
      var av = a[key], bv = b[key];
      if (typeof av === "string") return av.localeCompare(bv) * dir;
      return (av - bv) * dir;
    });
    tbody.innerHTML = "";
    rows.slice(0, state.maxRows).forEach(function (f) {
      var tr = document.createElement("tr");
      tr.innerHTML =
        '<td class="path">' + esc(f.path) + '</td><td>' + esc(f.language) + '</td>' +
        '<td class="num">' + fmt(f.code_lines) + '</td><td class="num">' + fmt(f.comment_lines) + '</td>' +
        '<td class="num">' + f.functions + '</td><td class="num">' + f.cyclomatic + '</td>' +
        '<td class="num">' + f.max_nesting + '</td><td class="num">' + f.issues + '</td>';
      tbody.appendChild(tr);
    });
  }

  function renderLicenses(l) {
    var box = el("licensePanel");
    if (!box || !l) return;
    var with_hdr = l.files_with_header || 0;
    var without  = l.files_without_header || 0;
    var total = with_hdr + without;
    box.innerHTML = "";
    if (total === 0) return;
    var pct = total > 0 ? Math.round(100 * with_hdr / total) : 0;
    var row = document.createElement("div"); row.className = "lic-row";
    row.innerHTML = '<span class="lic-pct">' + pct + '%</span>' +
      '<span class="lic-bar"><span style="width:' + pct + '%;background:#36c98d"></span></span>' +
      '<span class="lic-note">' + esc(String(with_hdr)) + " / " + esc(String(total)) + " files have a license header</span>";
    box.appendChild(row);
    var spdx = l.spdx || {};
    Object.keys(spdx).sort().forEach(function (k) {
      var d = document.createElement("div"); d.className = "lic-spdx";
      d.textContent = k + " (" + spdx[k] + ")";
      box.appendChild(d);
    });
  }

  /* ---- Directory browser ---- */
  function openBrowser(startPath) {
    el("browseModal").hidden = false;
    loadBrowse(startPath || "");
  }
  function closeBrowser() { el("browseModal").hidden = true; }
  function loadBrowse(path) {
    var enc = path ? "?path=" + encodeURIComponent(path) : "?path=/";
    api("/api/browse" + enc).then(function (d) {
      var pathEl = el("browsePath");
      var listEl = el("browseList");
      pathEl.textContent = d.path || "(drives)";
      listEl.innerHTML = "";
      /* Drives (Windows root level) */
      if (d.drives && d.drives.length) {
        d.drives.forEach(function (drv) {
          var row = document.createElement("div"); row.className = "browse-item";
          row.innerHTML = '<span class="browse-icon">&#x1F4BE;</span><span>' + esc(drv) + '</span>';
          row.addEventListener("click", function () { loadBrowse(drv); });
          listEl.appendChild(row);
        });
        return;
      }
      /* Parent ".." link */
      if (d.parent !== undefined && d.parent !== "") {
        var up = document.createElement("div"); up.className = "browse-item up";
        up.innerHTML = '<span class="browse-icon">&#x2B06;</span><span>..</span>';
        up.addEventListener("click", function () { loadBrowse(d.parent); });
        listEl.appendChild(up);
      } else if (d.path) {
        /* At drive root on Windows -- offer to go to drives list */
        var up2 = document.createElement("div"); up2.className = "browse-item up";
        up2.innerHTML = '<span class="browse-icon">&#x2B06;</span><span>Drives</span>';
        up2.addEventListener("click", function () { loadBrowse(""); });
        listEl.appendChild(up2);
      }
      /* Subdirectories */
      var sorted = (d.dirs || []).slice().sort();
      if (!sorted.length) {
        var empty = document.createElement("div");
        empty.className = "browse-item"; empty.style.color = "var(--muted)";
        empty.textContent = "(no subdirectories)";
        listEl.appendChild(empty);
      }
      sorted.forEach(function (name) {
        var fullPath = (d.path ? d.path.replace(/\\/g, "/").replace(/\/$/, "") + "/" : "") + name;
        var row = document.createElement("div"); row.className = "browse-item";
        row.innerHTML = '<span class="browse-icon">&#x1F4C1;</span><span>' + esc(name) + '</span>';
        row.addEventListener("click", function () { loadBrowse(fullPath); });
        listEl.appendChild(row);
      });
    }).catch(function () {
      el("browseList").innerHTML = '<div class="browse-item" style="color:var(--muted)">Cannot read directory.</div>';
    });
  }

  function renderPlatform(issues) {
    var box = el("platformPanel");
    if (!box) return;
    /* Bucket issues by platform target. */
    var win = [], posix = [], general = [];
    (issues || []).forEach(function (i) {
      var id = i.rule_id || "";
      if (id.indexOf("PORT-WIN-") === 0) win.push(i);
      else if (id.indexOf("PORT-POSIX-") === 0) posix.push(i);
      else if (id.indexOf("PORT-") === 0) general.push(i);
    });
    if (!win.length && !posix.length && !general.length) {
      box.innerHTML = '<p class="empty">No platform-specific issues detected. Code appears fully cross-platform.</p>';
      return;
    }
    box.innerHTML = "";
    function makeGroup(label, icon, color, items) {
      if (!items.length) return;
      var sec = document.createElement("div"); sec.className = "plat-group";
      var h = document.createElement("div"); h.className = "plat-heading";
      h.innerHTML = '<span class="plat-icon" style="background:' + color + '">' + icon + '</span>' +
        '<span class="plat-label">' + esc(label) + '</span>' +
        '<span class="plat-count">' + items.length + ' issue' + (items.length !== 1 ? 's' : '') + '</span>';
      sec.appendChild(h);
      var ul = document.createElement("ul"); ul.className = "plat-list";
      var shown = Math.min(items.length, 8);
      for (var j = 0; j < shown; j++) {
        var li = document.createElement("li");
        var f = (items[j].file || "").split("/"); f = f[f.length - 1];
        li.innerHTML = '<span class="plat-rule">' + esc(items[j].rule_id) + '</span> ' +
          '<span class="plat-file">' + esc(f) + '</span>' +
          '<span class="plat-line"> :' + (items[j].line || 0) + '</span>';
        ul.appendChild(li);
      }
      if (items.length > shown) {
        var more = document.createElement("li"); more.className = "plat-more";
        more.textContent = "... and " + (items.length - shown) + " more (filter by Portability in Issues)";
        ul.appendChild(more);
      }
      sec.appendChild(ul);
      box.appendChild(sec);
    }
    makeGroup("Windows-only constructs", "WIN", "#3b82f6", win);
    makeGroup("POSIX / Linux / macOS only", "NIX", "#10b981", posix);
    makeGroup("Cross-platform portability", "ALL", "#f59e0b", general);
  }

  function renderTrend(history) {
    if (!history || history.length < 2) { el("trendEmpty").hidden = false; el("trendCharts").hidden = true; return; }
    el("trendEmpty").hidden = true; el("trendCharts").hidden = false;
    var labels = history.map(function (h) { return h.timestamp.replace("T", " ").replace("Z", ""); });
    makeChart("tqiTrend", {
      type: "line",
      data: { labels: labels, datasets: [{ label: "TQI", data: history.map(function (h) { return +h.tqi.toFixed(1); }),
        borderColor: "#3fd0c9", backgroundColor: "rgba(63,208,201,0.12)", tension: 0.3, fill: true, pointRadius: 3 }] },
      options: { plugins: { legend: { display: false } }, maintainAspectRatio: false, scales: { y: { min: 0, max: 100 } } }
    });
    makeChart("issueTrend", {
      type: "line",
      data: { labels: labels, datasets: [{ label: "Issues", data: history.map(function (h) { return h.issue_count; }),
        borderColor: "#ef9b4a", backgroundColor: "rgba(239,155,74,0.12)", tension: 0.3, fill: true, pointRadius: 3 }] },
      options: { plugins: { legend: { display: false } }, maintainAspectRatio: false, scales: { y: { min: 0 } } }
    });
  }

  function renderDuplicates(rep) {
    var list = el("dupList");
    var groups = (rep && rep.duplicates) || [];
    el("dupEmpty").hidden = groups.length > 0;
    list.innerHTML = "";
    groups.forEach(function (g) {
      var div = document.createElement("div");
      div.className = "dup-group";
      div.textContent = g.join("  =  ");
      list.appendChild(div);
    });
  }

  function renderDependencies(files) {
    var table = el("depTable");
    var tbody = table.querySelector("tbody");
    tbody.innerHTML = "";
    var withDeps = (files || []).filter(function (f) { return f.dependencies && f.dependencies.length; });
    el("depEmpty").hidden = withDeps.length > 0;
    table.hidden = withDeps.length === 0;
    withDeps.forEach(function (f) {
      var tr = document.createElement("tr");
      tr.innerHTML = '<td class="path">' + esc(f.path.split("/").pop()) + '</td>' +
                     '<td>' + esc(f.dependencies.join(", ")) + '</td>';
      tbody.appendChild(tr);
    });
  }

  function renderLogs() {
    var tbody = el("logsTable").querySelector("tbody");
    tbody.innerHTML = ""; el("logsEmpty").hidden = state.logs.length > 0;
    state.logs.slice().reverse().forEach(function (e) {
      var tr = document.createElement("tr");
      tr.innerHTML =
        '<td class="path">' + esc(e.timestamp) + '</td>' +
        '<td><span class="pill ' + esc(levelClass(e.level)) + '">' + esc(e.level) + '</span></td>' +
        '<td>' + esc(e.message) + '</td>';
      tbody.appendChild(tr);
    });
  }

  function levelClass(l) {
    if (l === "ERROR") return "critical";
    if (l === "WARN") return "major";
    if (l === "DEBUG") return "info";
    return "minor";
  }
  function cap(s) { return s.charAt(0).toUpperCase() + s.slice(1); }
  function esc(s) {
    return String(s).replace(/[&<>"]/g, function (c) {
      return { "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" }[c];
    });
  }

  /* ---- v2.3.0: Technologies panel ---- */
  function renderTechnologies(techs) {
    var box = el("techPanel");
    if (!box) return;
    if (!techs || techs.length === 0) {
      box.innerHTML = '<p class="empty">No frameworks or libraries detected.</p>';
      return;
    }
    var cats = {};
    techs.forEach(function (t) {
      var c = t.category || "Other";
      if (!cats[c]) cats[c] = [];
      cats[c].push(t);
    });
    box.innerHTML = "";
    Object.keys(cats).sort().forEach(function (cat) {
      var sec = document.createElement("div");
      sec.className = "tech-group";
      var h = document.createElement("div");
      h.className = "tech-heading";
      h.textContent = cat;
      sec.appendChild(h);
      var ul = document.createElement("ul");
      ul.className = "tech-list";
      cats[cat].forEach(function (t) {
        var li = document.createElement("li");
        li.className = "tech-item";
        li.innerHTML = '<span class="tech-name">' + esc(t.name) + '</span>' +
          '<span class="tech-files">' + fmt(t.file_count) +
          ' file' + (t.file_count !== 1 ? 's' : '') + '</span>';
        ul.appendChild(li);
      });
      sec.appendChild(ul);
      box.appendChild(sec);
    });
  }

  /* ---- v2.3.0: Quality Gate panel ---- */
  function renderGate(g) {
    var box = el("gatePanel");
    if (!box) return;
    if (!g || !g.enabled) {
      box.innerHTML = '<p class="empty">Quality gate is disabled. Enable via ' +
        '<code>gate.enabled = true</code> in <code>code_analyzer.pson</code>.</p>';
      return;
    }
    var color = g.passed ? "#10b981" : "#e0586a";
    var label = g.passed ? "PASS" : "FAIL";
    box.innerHTML =
      '<div class="gate-result" style="border-color:' + color + '">' +
        '<span class="gate-badge" style="background:' + color + '">' + label + '</span>' +
        '<span class="gate-reason">' +
          esc(g.reason || (g.passed ? "All thresholds met." : "Threshold exceeded.")) +
        '</span>' +
      '</div>' +
      '<div class="tls-grid" style="margin-top:10px">' +
        '<div class="tls-row"><span class="tls-key">TQI</span><span class="tls-val">' +
          (g.tqi !== undefined ? (+g.tqi).toFixed(1) : "--") +
          ' (min: ' + (g.min_tqi !== undefined ? g.min_tqi : "--") + ')</span></div>' +
        '<div class="tls-row"><span class="tls-key">Issues</span><span class="tls-val">' +
          fmt(g.issues) + ' (max: ' + fmt(g.max_issues) + ')</span></div>' +
        '<div class="tls-row"><span class="tls-key">Critical</span><span class="tls-val">' +
          fmt(g.critical) + ' (max: ' + fmt(g.max_critical) + ')</span></div>' +
      '</div>';
  }

  /* ---- v2.3.0: Server health ping ---- */
  function pingHealth() {
    var dot = el("healthDot");
    if (!dot) return;
    fetch("/api/health", { credentials: "include" })
      .then(function (r) {
        dot.className = "health-dot " + (r.ok ? "up" : "down");
        dot.title = r.ok ? "Server online" : "Server error";
      })
      .catch(function () {
        dot.className = "health-dot down";
        dot.title = "Server unreachable";
      });
  }

  /* ---- v2.3.0: Issue status lifecycle ---- */
  function setIssueStatus(key, status, note, onDone) {
    api("/api/issue/status", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ key: key, status: status, note: note || "" })
    }).then(function () {
      if (onDone) onDone();
    }).catch(function (e) { console.warn("Issue status update failed:", e); });
  }

  /* ---- v2.3.0: Hash-password utility ---- */
  function wireHashPassword() {
    var btn = el("hashPwBtn");
    var inp = el("hashPwInput");
    var out = el("hashPwOut");
    if (!btn || !inp || !out) return;
    btn.addEventListener("click", function () {
      var pw = inp.value.trim();
      if (!pw) { out.textContent = "Enter a password first."; return; }
      api("/api/hash-password?password=" + encodeURIComponent(pw))
        .then(function (d) {
          out.textContent = d.sha256 || d.hash || JSON.stringify(d);
        }).catch(function (e) { out.textContent = "Error: " + e.message; });
    });
  }

  function loadSecurity() {
    /* Use raw fetch - /api/security is open (no auth required) but we bypass
     * the api() helper's 401->showLogin redirect to be safe. */
    fetch("/api/security", { credentials: "include" })
      .then(function (r) { return r.ok ? r.json() : null; })
      .then(function (s) { renderSecurity(s); })
      .catch(function () { renderSecurity(null); });
  }

  /* ---- data ---- */
  /* ---- Data load and session functions ---- */
  function loadAll() {
    return Promise.all([api("/api/results"), api("/api/files"), api("/api/issues")])
      .then(function (res) {
        state.lastReport = res[0]; state.files = res[1]; state.issues = res[2];
        renderHealth(state.lastReport.health);
        renderStats(state.lastReport);
        renderCharts(state.lastReport, state.issues, state.files);
        renderIssues(); renderFiles();
        renderDuplicates(state.lastReport); renderDependencies(state.files);
        renderPlatform(state.issues);
        api("/api/history").then(function (h) { state.history = h || []; renderTrend(state.history); }).catch(function () { /* best-effort; non-fatal */ });
        api("/api/licenses").then(function (l) { renderLicenses(l); }).catch(function () { /* best-effort; non-fatal */ });
        /* v2.3.0: technologies and quality gate */
        api("/api/technologies").then(function (t) { renderTechnologies(t); }).catch(function () { /* best-effort; non-fatal */ });
        api("/api/gate").then(function (g) { renderGate(g); }).catch(function () { /* best-effort; non-fatal */ });
        return loadLogs();
      }).catch(function (e) { if (e.message !== "unauthorized") console.error(e); });
  }

  function loadLogs() {
    return api("/api/logs?limit=200").then(function (logs) {
      state.logs = logs || []; renderLogs();
    }).catch(function () { /* best-effort; non-fatal */ });
  }

  /* ---- auth ---- */
  function showLogin(show) {
    el("login").hidden = !show;
    el("logout").hidden = show;
  }

  function checkSession() {
    return api("/api/session").then(function (s) {
      if (s.auth_required && !s.authenticated) { showLogin(true); return false; }
      showLogin(false);
      el("logout").hidden = !s.auth_required;
      return true;
    });
  }

  /* ---- appearance ---- */
  function applyTheme(mode) {
    document.documentElement.setAttribute("data-theme", mode);
    var btn = el("theme");
    if (btn) btn.textContent = (mode === "light") ? "Light" : "Dark";
    setCookie("mctheme", mode);
    if (state.lastReport) renderCharts(state.lastReport, state.issues, state.files);
    /* Re-render trend from cached history -- do NOT call api() here because
     * applyTheme runs at page load before authentication completes. */
    if (state.history && state.history.length) renderTrend(state.history);
  }

  function applyFontScale(pct) {
    pct = Math.max(80, Math.min(140, pct));
    state.fontScale = pct;
    document.documentElement.style.fontSize = (pct / 100 * 14) + "px";
    setCookie("mcfont", String(pct));
  }

  /* ---- wiring ---- */
  /* ---- Event wiring ---- */
  function wire() {
    el("theme").addEventListener("click", function () {
      var cur = document.documentElement.getAttribute("data-theme");
      applyTheme(cur === "light" ? "dark" : "light");
    });
    el("textDec").addEventListener("click", function () { applyFontScale(state.fontScale - 10); });
    el("textReset").addEventListener("click", function () { applyFontScale(100); });
    el("textInc").addEventListener("click", function () { applyFontScale(state.fontScale + 10); });
    el("pdf").addEventListener("click", function () { window.open("/api/report.pdf",  "_blank"); });
    el("json").addEventListener("click", function () { window.open("/api/report.json", "_blank"); });
    el("csv").addEventListener("click",  function () { window.open("/api/report.csv",  "_blank"); });
    el("browseBtn") && el("browseBtn").addEventListener("click", function () {
      var cur = el("root").value || ".";
      openBrowser(cur);
    });
    el("browseClose")  && el("browseClose").addEventListener("click",  closeBrowser);
    el("browseCancel") && el("browseCancel").addEventListener("click", closeBrowser);
    el("browseSelect") && el("browseSelect").addEventListener("click", function () {
      var p = el("browsePath").textContent;
      if (p) { el("root").value = p; }
      closeBrowser();
    });

    el("loginForm").addEventListener("submit", function (ev) {
      ev.preventDefault();
      el("loginErr").hidden = true;
      api("/api/login", {
        method: "POST", headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ username: el("loginUser").value, password: el("loginPass").value })
      }).then(function () {
        el("loginPass").value = ""; showLogin(false); el("logout").hidden = false;
        loadSecurity(); loadAll();
      }).catch(function () { el("loginErr").hidden = false; });
    });

    el("logout").addEventListener("click", function () {
      api("/api/logout", { method: "POST" }).finally(function () { showLogin(true); });
    });

    el("scan").addEventListener("click", function () {
      var btn = el("scan");
      var prog = el("scanProgress");
      var label = el("scanProgressLabel");
      var root = (el("root") && el("root").value) ? el("root").value : ".";
      btn.disabled = true; btn.textContent = t("analyzing") || "Analyzing...";
      if (prog) prog.hidden = false;

      var resetUI = function () {
        btn.disabled = false; btn.textContent = t("analyze") || "Analyze";
        if (prog) prog.hidden = true;
        if (label) label.textContent = "";
      };

      /* Hard safety timeout: no matter what happens with the WebSocket or the
       * fallback POST, the button is guaranteed to reset after 20 seconds. */
      var hardTimeout = setTimeout(resetUI, 20000);

      /* v2.5.0: Use WebSocket for live scan progress; fall back to POST if WS fails. */
      var wsProto = (location.protocol === "https:") ? "wss" : "ws";
      var wsUrl = wsProto + "://" + location.host + "/api/scan/ws?root=" + encodeURIComponent(root);
      var ws = null;
      var wsOk = false;

      try {
        ws = new WebSocket(wsUrl);
        ws.onopen = function () { wsOk = true; };
        ws.onmessage = function (ev) {
          try {
            var msg = JSON.parse(ev.data);
            if (msg.event === "start") {
              if (label) label.textContent = "Scanning " + (msg.root || "...") + "...";
            } else if (msg.event === "progress") {
              if (label) label.textContent = "Scanned " + (msg.n || "?") + " / " + (msg.total || "?") + " files...";
            } else if (msg.event === "done") {
              clearTimeout(hardTimeout);
              if (label) label.textContent = msg.files + " files, " + msg.issues + " issues, TQI " + msg.tqi;
              loadAll();
              setTimeout(resetUI, 1800);
            }
          } catch (e) { /* ignore parse errors */ }
        };
        ws.onerror = function () {
          if (!wsOk) { fallbackScan(); return; }
          clearTimeout(hardTimeout);
          resetUI();
        };
        ws.onclose = function () {
          if (!wsOk) { fallbackScan(); return; }
          clearTimeout(hardTimeout);
          resetUI();
        };
      } catch (e) {
        fallbackScan();
      }

      /* Fallback: plain POST /api/scan */
      function fallbackScan() {
        if (label) label.textContent = "Scanning (HTTP)...";
        api("/api/scan", {
          method: "POST", headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ root: root })
        }).then(loadAll).catch(function () { /* non-fatal */ }).finally(function () {
          clearTimeout(hardTimeout);
          resetUI();
        });
      }
    });

    el("shutdown").addEventListener("click", function () {
      if (!confirm(t("confirmShutdown"))) return;
      api("/api/shutdown", { method: "POST" }).catch(function () { /* best-effort; non-fatal */ });
      el("shutdown").textContent = t("signedOut");
    });

    el("srcClose").addEventListener("click", function () { el("srcPreview").hidden = true; });

    el("aiRun").addEventListener("click", function () {
      var out = el("aiOut");
      out.hidden = false;
      out.textContent = t("aiRunning");
      fetch("/api/ai/summarize", { method: "POST" }).then(function (r) {
        if (r.status === 401) { showLogin(true); throw new Error("unauthorized"); }
        return r.json();
      }).then(function (d) {
        if (d.ok && d.response) {
          out.textContent = d.response;
        } else {
          out.textContent = (d.error || "AI request failed") +
            (d.status ? " (HTTP " + d.status + ")" : "");
        }
      }).catch(function (e) {
        out.textContent = "AI request failed: " + e.message;
      });
    });

    el("logRefresh").addEventListener("click", loadLogs);
    el("logClear").addEventListener("click", function () {
      api("/api/logs/clear", { method: "POST" }).then(loadLogs).catch(function () { /* best-effort; non-fatal */ });
    });
    el("logDownload").addEventListener("click", function () {
      var text = state.logs.map(function (e) {
        return e.timestamp + " " + e.level + " " + e.message;
      }).join("\n");
      var blob = new Blob([text], { type: "text/plain" });
      var a = document.createElement("a");
      a.href = URL.createObjectURL(blob);
      a.download = "metis-code-analyzer-log.txt"; a.click();
      URL.revokeObjectURL(a.href);
    });

    Array.prototype.forEach.call(el("sevFilters").children, function (b) {
      b.addEventListener("click", function () {
        Array.prototype.forEach.call(el("sevFilters").children, function (x) { x.classList.remove("active"); });
        b.classList.add("active"); state.sevFilter = b.getAttribute("data-sev");
        setCookie("mcsev", state.sevFilter); renderIssues();
      });
    });
    /* Restore persisted severity filter and files sort. */
    var savedSev = getCookie("mcsev");
    if (savedSev) {
      state.sevFilter = savedSev;
      Array.prototype.forEach.call(el("sevFilters").children, function (x) {
        x.classList.toggle("active", x.getAttribute("data-sev") === savedSev);
      });
    }
    var savedSort = getCookie("mcsort");
    if (savedSort) {
      var parts = savedSort.split(":");
      if (parts.length === 2) { state.sort.key = parts[0]; state.sort.dir = parseInt(parts[1], 10) || -1; }
    }

    Array.prototype.forEach.call(el("filesTable").querySelectorAll("th[data-sort]"), function (th) {
      th.addEventListener("click", function () {
        var k = th.getAttribute("data-sort");
        if (state.sort.key === k) state.sort.dir *= -1;
        else { state.sort.key = k; state.sort.dir = (k === "path" || k === "language") ? 1 : -1; }
        setCookie("mcsort", state.sort.key + ":" + state.sort.dir);
        renderFiles();
      });
    });

    var rzTimer = null;
    window.addEventListener("resize", function () {
      if (rzTimer) clearTimeout(rzTimer);
      rzTimer = setTimeout(function () {
        if (state.lastReport) renderCharts(state.lastReport, state.issues, state.files);
      }, 150);
    });
    /* v2.3.0: hash-password utility panel */
    wireHashPassword();
  }

  document.addEventListener("DOMContentLoaded", function () {

    applyTheme(getCookie("mctheme") || "dark");
    var savedFont = parseInt(getCookie("mcfont"), 10);
    applyFontScale(isNaN(savedFont) ? 100 : savedFont);
    api("/api/version").then(function (v) {
      var label = "v" + v.version + " \u2013 " + v.rules + " rules";
      if (v.rules_active !== undefined && v.rules_active < v.rules)
        label += " (" + v.rules_active + " active)";
      el("ver").textContent = label;
    }).catch(function () { /* best-effort; non-fatal */ });
    api("/api/languages").then(function (langs) {
      state.languages = langs || []; renderLanguages();
    }).catch(function () { /* best-effort; non-fatal */ });
    fetch("/api/security", { credentials: "include" })
      .then(function (r) { return r.ok ? r.json() : null; })
      .then(function (s) { renderSecurity(s); })
      .catch(function () { renderSecurity(null); /* show dimmed badges on any failure */ });

    api("/api/config").then(function (c) {
      el("root").value = c.scan_root || ".";
      if (c.grade_thresholds) state.grades = c.grade_thresholds;
      if (c.max_table_rows) state.maxRows = c.max_table_rows;
      populateLanguages(c.locales || ["en-US"], c.default_locale || "en-US");
      applyI18n();
      /* Populate AI config panel (new in v2.2.0). */
      function setTxt(id, v) { var e = el(id); if (e) e.textContent = (v !== undefined && v !== null && v !== "") ? String(v) : "--"; }
      var ai = c.ai || {};
      setTxt("aiEnabled",   ai.enabled  ? "Yes" : "No");
      setTxt("aiProvider",  ai.provider  || (ai.endpoint ? ai.endpoint.replace(/\/v\d.*$/, "") : "--"));
      setTxt("aiModel",     ai.model     || "--");
      setTxt("aiMaxTokens", ai.max_output_tokens !== undefined ? ai.max_output_tokens : "--");
      setTxt("aiTimeout",   ai.timeout_seconds   !== undefined ? ai.timeout_seconds   : "--");
      setTxt("aiMaxIssues", ai.max_report_issues !== undefined ? ai.max_report_issues : "--");
    }).catch(function () { populateLanguages(["en-US"], "en-US"); applyI18n(); });

    /* Project selector is now populated after auth - see checkSession().then() below */

    wire();
    checkSession().then(function (ok) {
      if (ok) {
        loadSecurity();
        /* Load projects only after auth - it requires a valid session */
        fetch("/api/projects", { credentials: "include" })
          .then(function (r) { return r.ok ? r.json() : []; })
          .then(function (projects) {
            var sel = el("projSelect");
            var rootInput = el("root");
            if (!sel) return;
            if (!projects || projects.length === 0) { sel.hidden = true; return; }
            sel.innerHTML = "";
            projects.forEach(function (p) {
              var opt = document.createElement("option");
              opt.value = p.name; opt.textContent = p.name;
              if (p.active) opt.selected = true;
              sel.appendChild(opt);
            });
            sel.hidden = false;
            if (rootInput) rootInput.hidden = true;
            sel.addEventListener("change", function () {
              api("/api/project/select", {
                method: "POST", headers: {"Content-Type": "application/json"},
                body: JSON.stringify({name: sel.value})
              }).then(function () { loadAll(); }).catch(function (e) {
                if (e && e.message !== "unauthorized") console.warn("Project switch failed:", e);
              });
            });
          }).catch(function () {
            var sel = el("projSelect");
            if (sel) sel.hidden = true;
          });
        /* Load infrastructure status after initial data load completes. */
        function loadInfra() {
          [["/api/gpu", "pgpu"], ["/api/kubernetes", "pk8s"], ["/api/containers", "pctr"]].forEach(function (p) {
            api(p[0]).then(function (d) {
              var span = el(p[1]);
              if (!span) return;
              span.textContent = d.status || "unknown";
              span.classList.remove("planned", "enabled", "disabled");
              if (d.status === "planned")   span.classList.add("planned");
              if (d.status === "available") span.classList.add("enabled");
              if (d.status === "disabled")  span.classList.add("disabled");
              span.title = d.note || "";
            }).catch(function () {
              var span = el(p[1]);
              if (span) { span.textContent = "unavailable"; span.title = "Could not reach endpoint."; }
            });
          });
        }
        loadAll().then(loadInfra).catch(loadInfra);
      }
    });
    /* health ping - status dot in header */
    pingHealth();
    setInterval(pingHealth, 30000);
  });
})();
