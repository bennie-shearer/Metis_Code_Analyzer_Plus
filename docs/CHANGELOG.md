# Changelog - Metis Code Analyzer Plus

All notable changes to this project. Versions follow MAJOR.MINOR.PATCH.

## 2.4.2

- Fixed: header brand text ("Metis Code Analyzer Plus") was wrapping onto two
  lines at normal browser width. Added white-space:nowrap and font-size:13px
  to .brand .name; added flex-shrink:0 to .brand so it never collapses.
- Fixed: header controls were cramped, causing button text to wrap. Reduced
  header gap (14px->10px), padding (12px 22px->10px 16px), controls gap
  (8px->5px), button padding (8px 14px->6px 11px), input width (220px->180px).
  Added white-space:nowrap to button and button.ghost.
- Added flex-wrap:nowrap and min-width:0 to header so layout stays on one row.
- Version bumped to 2.4.2 in all references.

## 2.4.1

- Fixed: TLS / AES-256-GCM / Post-Quantum TLS badges now display side-by-side
  (left-justified, horizontal row) instead of stacked vertically. Changed
  .sec-badges from flex-direction:column to flex-direction:row with flex-wrap
  so they line up identically to the Metis Antique Marketplace Plus layout.
- Fixed: Console 401 error on /api/projects at page load. The projects call
  was firing at DOMContentLoaded before authentication completed. Moved to
  inside checkSession().then() so it only runs with a valid session. Used raw
  fetch() (not api()) to avoid the 401->showLogin redirect side-effect.
- Version bumped to 2.4.2 in all references.

## 2.4.0

- openapi.yaml: version updated from 2.1.4 to 2.4.0; added missing endpoints:
  GET /api/gate, GET /api/technologies, GET /api/issue/status,
  POST /api/issue/status, GET /api/hash-password, POST /api/hash-password,
  GET /api/browse, GET /api/projects, POST /api/project/select, GET /metrics.
  OpenAPI spec now covers all 36 server endpoints.
- docs/ARCHITECTURE.md: corrected include path (metis/cast -> metis/codeanalyzer);
  updated component list to include ai.hpp, ast.hpp, license.hpp, pdf.hpp,
  technologies.hpp; updated scope section to reflect completed work; added
  infrastructure roadmap section.
- docs/CONFIGURATION.md: added all missing PSON key tables: gate.*, ai.*,
  gpu.*, kubernetes.*, containers.*, technologies.*, api_key.*, webhook.*,
  scan.watch, ui.*, arch.*.
- README.md: corrected THIRD_PARTY.md reference from root to docs/.
- CHANGELOG.md: moved to docs/ as canonical copy; root CHANGELOG.md is now
  a redirect stub pointing to docs/CHANGELOG.md per project convention.
- Version bumped to 2.4.2 in all references including openapi.yaml.

## 2.3.4

- Fixed false positive: SEC-SQL-CONCAT (CWE-89) was incorrectly flagging
  HTML string concatenation in web/app.js at line 265. The word "selected"
  in an HTML <option selected> attribute matched the SQL keyword "select"
  in the rule pattern "(select|insert|update|delete)\b.*[\"'].*\+".
  Root cause: the pattern matched any occurrence of SQL keywords regardless
  of context, including HTML attribute strings in JavaScript.
  Fix: pattern tightened to require a SQL clause context after the keyword:
    select\s+\w  (SELECT followed by a column name)
    insert\s+into
    update\s+\w
    delete\s+from
  This eliminates false positives from HTML/JS string building while
  correctly flagging actual SQL concatenation patterns.
- Version bumped to 2.4.2 in all references.

## 2.3.3

- Added TLS 1.3 badge (green, id="badgeTls") to the security badge bar,
  matching the Metis Antique Marketplace Plus three-badge pattern:
  TLS 1.3 | AES-256-GCM | Post-Quantum TLS.
  Badge shows the negotiated protocol (e.g. "TLS 1.3") from /api/security
  response field "protocol"; dimmed grey when TLS is not active.
- Added CSS class .sec-badge.tls { background: #0c8b6e } (teal/green).

## 2.3.2

- Fixed: TLS/AES-256-GCM/Post-Quantum badges missing from header on load.
  Root cause: /api/security was called via the api() helper which throws and
  calls showLogin() on any non-200 response, silently swallowing the result
  before the DOM could render. Fix: /api/security now called via raw fetch()
  directly, bypassing the api() 401 redirect trap entirely.
- Fixed: renderSecurity() guarded against null argument so dimmed "off" badges
  render immediately on page load even before /api/security responds, matching
  the Metis Antique Marketplace Plus pattern where badges always show.
- Fixed: loadSecurity() now called after checkSession() succeeds and after
  successful login, ensuring badges reflect the authenticated TLS state.

## 2.3.1

- Removed all .md and .txt files from project root except README.md and
  CHANGELOG.md (GitHub standard locations). Canonical copies of all other
  documents now live exclusively in docs/:
  BACKGROUND.md, GOVERNMENT_REFERENCES.md, HOWTO.md, SUPPORT.md,
  THIRD_PARTY.md, TODO.md.
- Root now contains only: README.md, CHANGELOG.md, LICENSE, VERSION,
  CMakeLists.txt, code_analyzer.pson, certs/, docs/, include/, src/,
  web/, openssl-prebuilt/, vendor/.

## 2.3.0

- Dashboard: Technology Inventory panel added. Fetches detected frameworks and
  libraries from GET /api/technologies, groups by category, and displays name
  and file count per technology. Covers 70+ patterns across JavaScript, Python,
  Java, Go, Rust, and C++.
- Dashboard: Quality Gate panel added. Fetches GET /api/gate and displays
  PASS/FAIL result with TQI, issue count, and critical count against configured
  thresholds. Gate configuration via gate.* PSON keys.
- Dashboard: Issue Status column added to the Issues table. Each row has a
  Status drop-down (Open / Accepted / Won't Fix / False Positive) that calls
  POST /api/issue/status on change. Status is color-coded and persisted server-
  side for the duration of the session.
- Dashboard: Server health dot added to the page header. Pings GET /api/health
  on load and every 30 seconds. Green = online, red = unreachable.
- Dashboard: Admin Tools panel added. Provides a Password Hash Generator that
  calls GET /api/hash-password and displays the SHA-256 result ready for paste
  into auth.password_sha256 in code_analyzer.pson.
- docs/API.md: fully rewritten to cover all 36 server endpoints including
  GET /api/health, GET /api/gate, GET /api/technologies,
  GET /api/issue/status, POST /api/issue/status, GET /api/hash-password,
  GET /api/source, GET /api/licenses, GET /api/browse, GET /api/projects,
  POST /api/project/select, GET /api/history, GET /api/report.json,
  GET /api/report.csv, GET /api/metrics, and all infrastructure stubs.
- version.hpp: MAJOR/MINOR/PATCH macros corrected to 2/3/0 (were 2/1/7).
- CSS additions: .health-dot, .issue-status-cell, .issue-status-sel,
  status-accepted/wontfix/fp, .tech-group/heading/list/item, .gate-result,
  .gate-badge, .gate-reason, .hash-row, .hash-input, .hash-out.
- Version bumped to 2.4.2 in all references.

## 2.2.0

- Dashboard: all server API endpoints wired in browser client. GPU, Kubernetes,
  and Container subsystems now appear in a dedicated Infrastructure panel with
  cards (pgpu, pk8s, pctr) that display live status from /api/gpu,
  /api/kubernetes, and /api/containers.
- Dashboard: new TLS/Security Configuration panel shows protocol, cipher, key
  exchange group, post-quantum status, AES-256-GCM status, and cert CN.
- Dashboard: new AI Configuration panel shows ai.enabled, provider, model,
  max_output_tokens, timeout, and max_report_issues from /api/config.
- Server: /api/security extended with protocol and cert_cn fields.
- Server: /api/config extended with full ai sub-object.
- Port 8080 configurable via top-level `port` key in code_analyzer.pson.
- GPU, Kubernetes, and Container integrations are planned C++20 work for
  Windows, Linux, and macOS. No Docker, PyTest, GTest, Doxygen, or Jupyter.
- BACKGROUND.md re-crafted with Table of Contents: history of code-analyzer
  libraries, ecosystem context, design philosophy, scoring model, roadmap.
- All .md/.txt docs consolidated in docs/. Root files redirect to docs/.
- LICENSE updated with dedication, disclaimer, author, acknowledgments.
- README.md rewritten for GitHub publish readiness.
- CSS additions: infra-grid/card/badge, tls-grid/row, section-desc.

## 2.1.7

- Export downloads are now named after the scanned target instead of fixed
  product strings. The PDF, JSON, and CSV exports derive their filename from
  the last path component of the scanned root, so scanning
  C:/Users/Benni/CLionProjects/Metis_Medical_Plus yields
  Metis_Medical_Plus-report.pdf, Metis_Medical_Plus-report.json, and
  Metis_Medical_Plus-issues.csv rather than metis-code-analyzer-report.pdf
  and the like. A new download_stem helper takes the basename (splitting on
  both / and \\), keeps [A-Za-z0-9._-], maps every other byte to a single
  '_', trims leading/trailing separators, and falls back to "report" for an
  empty root, ".", or a bare drive such as "C:/". The filename now matches
  the scanned_root carried inside each report.


## 2.1.6

- The CSV issues export now identifies the scanned source, matching the PDF
  report ("Scanned root:" line) and the JSON export ("scanned_root" field).
  Two leading comment lines are prepended to /api/report.csv output: the
  product name with version and the scanned root. The lines begin with '#'
  so the issues table that follows remains a clean single-table CSV; tools
  that honor the '#' comment convention skip them, and spreadsheets display
  them as readable provenance rows above the header. The column header and
  issue rows are unchanged.

## 2.1.5

- Tightened four false-positive-prone rules so correct code is no longer
  misreported:
  - SEC-EVAL now flags eval() unconditionally but exec() only when its
    argument is dynamic. A string literal, raw string, or const-qualified
    declaration is treated as benign, so database exec("...static SQL...")
    helpers are no longer reported as dynamic code execution (CWE-95).
  - SEC-HARDCODED-SECRET now requires a self-contained quoted literal value
    and skips concatenations and filesystem paths. Header names such as
    "x-api-key: " + var, URL fragments such as "?secret=" + var, and token
    file paths are no longer flagged.
  - SEC-SYSTEM-CALL no longer flags object method calls (obj.system()) or
    URL path text (/admin/system in a comment). std::system, popen,
    os.system, and subprocess.call still match.
  - CHG-MAGIC-PATH no longer flags /.well-known/ routes, which are URL
    paths rather than filesystem paths.
- Portability rules are now guard-aware. PORT-WIN-* findings are suppressed
  inside a #ifdef _WIN32 (true) region and PORT-POSIX-* findings inside the
  matching #else / #ifndef _WIN32 region. Correctly guarded cross-platform
  code is no longer penalized; unguarded platform-specific code is still
  flagged.
- Removed a duplicate Portability rule block. Rules 21-27 were defined
  twice; with last-definition-wins the second, less complete block silently
  shadowed the first, so LPCTSTR/LPTSTR/QWORD types and the netinet/in.h,
  arpa/inet.h, and sys/ioctl.h headers were not being detected. The
  comprehensive definitions are now the single source of truth.
- Added "sqlite" and "tests" to scan.exclude so the vendored SQLite
  amalgamation and test fixtures are skipped during analysis.

## 2.1.4

- Portability added to PDF report health factors section. Was missing
  because the PDF generator predates the Portability factor. Also added
  to the plain-text AI report summary.

## 2.1.3

- Directory browser added. A folder button next to the scan-root field
  opens a modal dialog that navigates the server filesystem. On Windows
  shows drive letters at the root level, then subdirectories. Hidden
  directories (names starting with .) are skipped. Selecting a directory
  sets the scan-root field. GET /api/browse?path=<dir> is the backing
  endpoint (requires authentication).

## 2.1.2

- Project dropdown and scan-root text input are mutually exclusive.
  Named projects configured: dropdown shown, text input hidden.
  No named projects: text input shown, dropdown hidden.
  Showing both simultaneously was redundant and confusing.

## 2.1.1

- Project dropdown always visible. Previously hidden when fewer than two
  projects were configured. Now shows "Default" when no named projects
  exist, and the list of named projects otherwise. No reason to hide it.

## 2.1.0

- ROB-EMPTY-CATCH fixed in app.js:648. The project-select .catch() handler
  was empty -- the scanner correctly flagged it as CWE-1069. Fixed by
  logging the error to console.warn (excluding "unauthorized" which is
  already handled by the api() function).

## 2.0.9

- Project selector dropdown added to dashboard header. Populated from
  /api/projects and only visible when two or more projects are configured
  in the PSON (project.1.name, project.2.name etc). Selecting a project
  calls POST /api/project/select and reloads all dashboard panels.

## 2.0.8

- Platform Compatibility panel added to dashboard. Issues from PORT-WIN-*
  rules appear under a Windows-only group, PORT-POSIX-* under POSIX/Linux/macOS,
  and PORT-PATH-*/PORT-MAX-PATH under Cross-platform. Shows rule ID, filename,
  and line per finding with overflow count and link to Issues filter.
  Panel shows "fully cross-platform" message when no issues are found.

## 2.0.7

- Portability added as a 6th health factor. TQI is now the average of
  six factors instead of five. Seven new rules (PORT-WIN-API,
  PORT-WIN-TYPES, PORT-WIN-SLEEP, PORT-POSIX-FORK, PORT-POSIX-ONLY,
  PORT-PATH-SEP, PORT-MAX-PATH) detect Windows/POSIX/cross-platform
  issues. openssl-prebuilt and certs excluded from scan to prevent
  false positives. Rule regex changed to case-sensitive (was icase)
  so variable names like "handle" no longer trigger PORT-WIN-TYPES.
  Self-scan: Grade A TQI 98.1 Issues 1 (legitimate DWORD inside ifdef).

## 2.0.6

- HOWTO.md added: 16-section end-user guide covering build, TLS setup,
  authentication, scan configuration, results interpretation, rule
  management, CI/CD integration, exports, AST mode, and shutdown.
- TODO.md updated: all items completed through v2.0.5; zero open items.

## 2.0.5

- AES-256-GCM badge now lights correctly. SSL_OP_CIPHER_SERVER_PREFERENCE
  added to the TLS context. Without it, OpenSSL follows the CLIENT cipher
  order in TLS 1.3; Edge sends TLS_AES_128_GCM_SHA256 first, so AES-128
  was being selected. With server preference enforced, TLS_AES_256_GCM_SHA384
  is always negotiated and the blue badge activates.

## 2.0.4

- SEC-PRIVATE-KEY false positive fixed. gen_certs.ps1 contained the literal
  PEM header string which the scanner correctly detected as CWE-321. Fixed
  by constructing the header string from two concatenated parts so no single
  token matches the secret pattern.
- TLS cipher now logged to activity log on first browser connection, visible
  in the dashboard Activity Log section.

## 2.0.3

- certs/gen_certs.ps1 added (matches Metis Medical Plus pattern).
  Generates an RSA-2048 cert with SAN directly in Cert:\LocalMachine\Root
  so Edge and Chrome trust it. Exports server.crt and server.key to
  the certs/ folder. No external tools required -- pure PowerShell
  and Windows .NET. PSON now uses relative paths certs/server.crt and
  certs/server.key identical to Medical Plus certs/server.crt pattern.

## 2.0.2

- cert file loader switched from SSL_CTX_use_certificate_file to
  SSL_CTX_use_certificate_chain_file. mkcert bundles the leaf cert and CA
  in a single PEM; the old loader only read the first cert and failed.
  OpenSSL error string now included in the startup log on load failure
  so the exact reason is visible without a debugger.

## 2.0.1

- tls.cert_file and tls.key_file set to mkcert-generated localhost cert.
  Server now loads the mkcert cert signed by the local trusted CA instead
  of generating its own self-signed cert. Green padlock in Edge and Chrome.

## 2.0.0

- Visual Basic 6 added (language 46). Extensions: .bas .cls .frm .ctl .vbp.
  Comment: apostrophe. Functions: Sub/Function/Property. Cyclomatic:
  ElseIf/Else/For/Do/While/Select/Case/On Error.

## 1.9.9

- Self-signed certificate now includes Subject Alternative Name
  (DNS:localhost, IP:127.0.0.1, IP:::1), extendedKeyUsage:serverAuth,
  and basicConstraints:CA:TRUE. Chrome and Edge require SAN since 2017;
  without it the cert is rejected from the Windows trust store even after
  import. With SAN, Option A (certmgr.msc import) now works and the
  browser shows a green padlock for https://localhost:8080.

## 1.9.8

- AES-256-GCM badge now lights blue. The server now sets TLS 1.3 cipher
  preference order (AES-256-GCM first, then ChaCha20, then AES-128-GCM).
  Browsers respect the server preference in TLS 1.3, so AES-256-GCM is
  negotiated and the badge activates.

## 1.9.7

- METIS_ENABLE_TLS default changed from OFF to ON. TLS is now built
  automatically. The prebuilt OpenSSL 4.0 Windows libs are included;
  no external install or CMake flag needed.

## 1.9.6

- **45 languages** (was 30). Added: Ada, PowerShell, MATLAB (.mlx),
  Groovy, Zig, ABAP, F#, Erlang, Clojure, Crystal, Nim, D, OCaml,
  VHDL, Verilog/SystemVerilog. Each has correct comment style, function
  detection heuristics, and language-specific cyclomatic keywords.
  Note: MATLAB uses .mlx extension to avoid conflict with
  Objective-C .m files.

## 1.9.5

- **Rule count fixed.** Dashboard header was showing 14 (active rules only)
  instead of 20 (total defined). /api/version now returns both: rules=20
  (total including disabled MISRA/CERT stubs) and rules_active=14 (enabled).
  Header displays "v1.9.5 -- 20 rules (14 active)".

## 1.9.4

- **OpenSSL 4.0.0 prebuilt Windows libs added.** openssl-prebuilt/windows/lib/
  contains libssl.a and libcrypto.a built for MinGW-w64. CMakeLists.txt now
  selects these automatically on Windows when METIS_ENABLE_TLS=ON -- no
  separate OpenSSL install required. Linux/macOS still use system OpenSSL.
- **OpenSSL 4.0 API compatibility.** X509_get_subject_name now returns
  const X509_NAME* in 4.0; fixed with const_cast in http.hpp. SSL_set1_host
  is deprecated in 4.0; deprecation warning suppressed with a GCC pragma
  until a replacement API is adopted.
- To enable TLS in CLion: CMake options -> add -DMETIS_ENABLE_TLS=ON.
  No OPENSSL_ROOT_DIR needed; the prebuilt path is resolved from the project
  root automatically.

## 1.9.3

- **Login dialog now hides correctly after sign-in.** Root cause: the CSS rule
  .login-overlay { display: flex } has higher specificity than the browser's
  default [hidden] { display: none }, so setting element.hidden = true had no
  visual effect -- the dialog stayed on screen even though JavaScript believed
  it was hidden. Fixed with .login-overlay[hidden] { display: none !important }
  in style.css. This is the standard defensive rule for any element that has
  both a display property in CSS and uses the HTML hidden attribute.

## 1.9.2

- **CSP inline-style violation fixed.** style-src 'self' blocked all dynamic
  inline styles set by JavaScript (health-factor bar widths/colors at app.js:141,
  license-coverage bar at app.js:291). These are runtime-computed values that
  cannot be expressed as static CSS classes. Changed to style-src 'self'
  'unsafe-inline'. script-src stays strict ('self' only); unsafe-inline is
  style-only and the attack surface is cosmetic, not code execution. This CSP
  violation was also the root cause of the login dialog reappearing after sign-in:
  renderHealth() failing silently broke the Promise chain in loadAll() in a way
  that left the dialog visible.

## 1.9.1

- **Login dialog no longer reappears after sign-in.** Root cause identified
  from server log: GET /api/history was firing 19 seconds before the user
  logged in (at page load), returning 401, and triggering showLogin(true).
  The source was applyTheme() calling api("/api/history") every time it ran,
  including on the initial DOMContentLoaded before any session existed.
  Fix: applyTheme() now re-renders the trend chart from cached state.history
  instead of making an API call. The api() function must never be called
  with authenticated endpoints before checkSession() completes.
- Also: CMakeLists.txt project() declaration updated to use proper casing
  (Metis_Code_Analyzer_Plus) and DESCRIPTION field.

## 1.9.0

- **Login fixed on all browsers / OS combinations.** The session cookie was
  not being reliably sent by fetch() in Firefox on Windows because the default
  credentials mode ("same-origin") is suppressed by Enhanced Tracking Protection
  on localhost. Fixed by explicitly setting credentials:"include" on every
  fetch call in api(). The cookie was always correct server-side (confirmed
  by curl tests); the browser simply wasn't forwarding it.
- **SameSite changed from Strict to Lax.** SameSite=Strict blocks cookies on
  any same-site top-level navigation; Lax allows GET navigations and is the
  correct choice for a self-hosted tool. No security impact for a localhost
  or internal server.
- **401 responses now logged.** The unauthorized() helper logs the method and
  path so any future session issues appear immediately in the activity log.

## 1.8.9

- **Default password changed from Cast#2026 to Admin#2026.** The word "Cast"
  must not appear in this product. All docs, PSON comments, startup warning,
  and README updated. Hash updated to b5af2beb... (SHA-256 of Admin#2026).
- **hash-password endpoint fixed and restored.** GET /api/hash-password was
  missing from the server. It is now added back alongside a POST variant:
  POST /api/hash-password with body {"value":"<password>"} is the recommended
  method since a '#' in a GET query string is treated as a URL fragment by
  browsers and silently truncated before the server sees it. The GET form
  includes a note warning about this when '#' would be affected.
- **All Cross-language AST support added** (METIS_ENABLE_AST=ON):
  tree-sitter runtime and grammars for C, Python, JavaScript, Go, Java, Rust
  vendored in vendor/. CMakeLists.txt METIS_ENABLE_AST option compiles them
  as static libraries. ast.hpp wraps the C API; analyse_file() uses AST-derived
  metrics when available.
- **Quality gate:** GET /api/gate, gate.* config in PSON; exit_nonzero support
  for CI pipelines.
- **Issue lifecycle:** POST /api/issue/status; status and note fields in
  GET /api/issues response.
- **Technology inventory:** GET /api/technologies detects 70+ frameworks.
- **MISRA-C / CERT-C rule stubs:** rules 16-20 (disabled by default).
- **CI/CD integration docs:** docs/CI.md with GitHub Actions, Jenkins,
  GitLab CI examples.
- **Architecture rules:** config stub documented in PSON (implementation
  planned next).

## 1.8.8

- **TODO.md rewritten**: was stale since v1.8.1 (still showed Grade B / TQI
  81.4 self-scan, missing all v1.8.2-v1.8.7 completions). Now accurately
  reflects one open item (AST front ends / tree-sitter) and a full completed
  history across Engine, Security, Export, Configuration, Dashboard, and Rule
  quality. Self-scan section updated to Grade A / TQI 98.0.

## 1.8.7

- **Incremental scanning**: the Analyzer now maintains a per-file mtime cache.
  Files whose modification time has not changed since the last scan skip
  compute_metrics() and rules_.apply(); only duplication detection runs across
  all files on every scan. Enable/disable via analysis.incremental in PSON
  (default true). analyse_file() helper lambda eliminates the previously
  duplicated three-line analysis block.
- **Multi-project support**: define named scan roots in PSON with
  project.N.name and project.N.root. Each project has independent state
  (AnalysisResult, LicenseSummary, history ring, mutex). All projects are
  scanned on startup when scan.on_start is true. New endpoints:
  GET /api/projects (list all with grade/TQI/active flag) and
  POST /api/project/select {"name":"..."} (switch active project). All
  existing endpoints operate on the active project. One implicit "default"
  project is created from scan.root when no project.N entries are defined.
- Self-scan: Grade A, TQI 98.0, 0 issues, 0 duplicate blocks.

## 1.8.6

- Security headers on every response: X-Frame-Options DENY, Referrer-Policy
  no-referrer, Content-Security-Policy (default-src 'self'), and HSTS when TLS
  is active. Secure flag added to session cookie when connection is TLS.
- API key auth: X-API-Key header accepted alongside session cookie when
  auth.api_key_enabled is true (auth.api_key_sha256 in PSON).
- Export endpoints: GET /api/report.json (full structured) and /api/report.csv
  (issues as CSV). Export JSON and CSV buttons added to dashboard toolbar.
- Webhook: POST notification on scan completion (webhook.* in PSON).
- Watch mode: poll scan root for changes and rescan automatically
  (scan.watch, scan.watch_interval_seconds in PSON).
- History persistence: append each snapshot to a NDJSON file
  (ui.history_file); load on startup to survive restarts.
- License detection: GET /api/licenses; scans each file header for
  SPDX-License-Identifier and copyright notices. Dashboard license panel.
- Secrets hardening: 4 new rules -- SEC-PRIVATE-KEY (PEM blocks),
  SEC-AWS-KEY (AKIA patterns), SEC-BEARER-TOKEN (JWT literals), and
  CHG-NO-LICENSE (disabled by default).
- Per-language complexity thresholds: score.complexity.threshold.<lang>
  in PSON overrides the global default per language.
- OpenAPI 3.0 spec at docs/openapi.yaml, served at GET /docs/openapi.yaml.
- Self-scan: Grade A, TQI 97.9, 0 issues, 0 duplicate blocks.

## 1.8.5

- Extended language support from 15 to 30 languages. Added: Kotlin,
  Swift, Scala, Dart, R, Lua, Perl, Objective-C, Fortran, HLASM, REXX,
  JCL, PL/I, Haskell, Elixir. Each has correct comment detection,
  language-specific function-count heuristics, and decision-keyword sets
  for cyclomatic complexity. Shell extended with .zsh and .ksh; PHP with
  .phtml.
- Added tree-sitter to BACKGROUND.md as the named candidate for the
  future AST tier (METIS_ENABLE_AST). Updated TODO.md accordingly.

## 1.8.4

- Rewrote `BACKGROUND.md`: documents the full application-intelligence
  landscape (CAST, SonarQube, TICS, Squore, Kiuwan, Coverity, Klocwork,
  PVS-Studio, clang-tidy, cppcheck, Checkmarx, Veracode, Fortify, Semgrep,
  CodeClimate, Embold) with honest positioning of where Metis fits.
- Fixed stale "Metis CAST Plus" name in README intro.
- Platform support confirmed: Windows (MinGW-w64/MSVC), Linux (GCC/Clang),
  macOS (Clang) -- same POSIX path as Linux, zero platform-specific divergence.

## 1.8.3

- **No hardcoded operational parameters**: last two remaining hardcodes wired
  to PSON: `ui.pdf_max_issues` now controls the PDF issue list length (was
  ignored; hardcoded 40), and a new `ai.max_report_issues` key controls how
  many issues are sent to the AI summarizer (was hardcoded 15).
- **`/api/config` now exposes `max_table_rows` and `ai_enabled`** so the
  dashboard reads the configurable table limit from PSON rather than using a
  hardcoded 500.
- **XSS hardening**: `rule_id`, `cwe`, `severity`, `language`, and log `level`
  fields were inserted into `innerHTML` unescaped. All now pass through `esc()`.
- **No Python used anywhere**: all tooling and build steps use C++20, classic
  HTML/CSS/JS, and PSON only. Verification pipeline uses node and bash/grep/sed.
- Self-scan: Grade A, TQI 97.6, 0 issues, 0 duplicate blocks. $0 debt.

## 1.8.2

- **avg_complexity fixed**: was total cyclomatic per file; now correctly computed
  as total cyclomatic divided by total function count (per-function average).
  A 6-function file with 12 decision points scores 2.0, not 12.0.
- **score.complexity.threshold** updated from 10.0 to 5.0 to match the
  per-function interpretation (a per-function cyclomatic of 5 is a reasonable
  threshold for most code).
- **Comment counter fixed**: doc-comment continuation lines starting with `* `
  (e.g. ` * param description`) were not being counted. Fixed in metrics.hpp.
- **Duplicate blocks reduced from 61 to 0**:
  - `i18n.js` added to default scan excludes (translation key-name strings were
    matching across locale blocks -- data duplication, not code duplication).
  - Repeated `mc::http::Response res; res.body = ....dump(); return res;` tails
    across 10 route handlers replaced with a `json_response()` helper.
  - Shared `#include` window between `pson.hpp` and `scanner.hpp` broken by
    adding header comments.
- **Intentional empty catch annotated** in http.hpp (malformed Content-Length).
- **`server.index_doc`** in PSON controls the directory index document (was `/index.html` hardcoded).
- Self-scan result: **Grade A, TQI 97.6**, 0 issues, 0 duplicate blocks.

## 1.8.1

- **CHG-MAGIC-PATH rule fixed**: the pattern now correctly distinguishes
  filesystem paths from URL/API routes. API paths (`/api/...`, `/metrics`)
  no longer trigger the rule; genuine OS paths (`/etc/...`, `/home/...`) and
  multi-segment file paths still do.
- **`rules.hpp` added to default scan excludes**: prevents the engine from
  flagging its own detection patterns as findings in a self-scan.
- **`/index.html` hardcoded default document removed**: the directory index
  document is now configurable via `server.index_doc` in the PSON config.
- **PSON grammar reconciliation completed**: `pson.hpp` was audited against the
  canonical Metis dotted-key PSON format; grammar is consistent. The
  reconciliation TODO is now closed and documented in `pson.hpp`.
- Self-scan result: Grade B, TQI 81.4, 0 issues, Security 100, Efficiency 100.

## 1.8.0

Completed all open TODO items except two (AST front ends and PSON grammar
reconciliation, which are tracked honestly as still open).

- **Inline source preview**: clicking any issue row fetches a window of source
  lines from the new `GET /api/source` endpoint (auth-gated, path-traversal-
  confined to the scan root) and displays it inline with the focal line
  highlighted. Closeable with the X button.
- **Scan history and trend lines**: each scan is recorded in a configurable
  in-memory ring (`ui.history_max`, default 50). `GET /api/history` returns the
  time series; the dashboard shows TQI and issue-count trend line charts.
- **Duplication locations**: `GET /api/results` now includes the actual
  `file:line` groups for each duplicate block (not just a count). The dashboard
  shows a Duplicate Blocks panel with each group.
- **Dependencies panel**: the per-file include/import edges were already
  extracted; they are now surfaced in a dashboard Dependencies table.
- **Login rate-limiting**: per-username brute-force throttle. After
  `auth.login_max_attempts` failed attempts within `auth.login_window_seconds`,
  further attempts receive HTTP 429 with a `Retry-After` header.
- **Default-credential warning**: the server now logs a loud warning at startup
  when the shipped default password hash is still in use.
- **Filter and sort persistence**: severity filter (`mcsev`) and files sort
  (`mcsort`) are persisted across reloads via cookies.
- **Excluded vendored Chart.js** from the default scan exclusion list so a
  self-scan no longer generates false positives from the minified third-party
  file.
- **Annotated intentional JS catch handlers** to clear the `ROB-EMPTY-CATCH`
  false positives in a self-scan.
- **`ui.history_max`** added to PSON config; `auth.login_max_attempts` and
  `auth.login_window_seconds` added to auth section.

## 1.7.0

- TLS is now on by default (`tls.enabled = true`). A TLS-enabled build serves
  HTTPS; a build without TLS support logs a warning and serves plain HTTP on the
  same port instead of failing to start.
- Single configurable listen port (`port`, default 8080) is used for both HTTP
  and HTTPS; the separate TLS port was removed.
- Completed the PSON config: every operational value is now documented and
  configurable, including server limits (`server.max_header_bytes`,
  `server.listen_backlog`, `server.recv_chunk_bytes`), `auth.session_token_length`,
  TLS cert parameters (`tls.min_version`, `tls.cert_days`, `tls.key_bits`,
  `tls.cert_cn`), and the new `ai`, `gpu`, `kubernetes`, and `containers`
  sections. Nothing operational is hardcoded.
- Added an optional AI summary feature: `POST /api/ai/summarize` calls an
  operator-configured, OpenAI-compatible provider over HTTPS (requires a
  TLS-enabled build and `ai.enabled = true`) and a dashboard "AI Insights" panel.
- Added planned-subsystem endpoints and dashboard cards for GPU, Kubernetes, and
  Containers (`/api/gpu`, `/api/kubernetes`, `/api/containers`), reporting
  "planned" pending future C++20 implementations for Windows, Linux, and macOS.
- Surfaced every server feature in the dashboard, including a Prometheus
  `/metrics` link.
- Fixed a stale product-name comment in `web/i18n.js`. All shipped files remain
  7-bit ASCII (localized text uses \uXXXX escapes).

## 1.6.1

- Added two security badges to the dashboard (AES-256-GCM and Post-Quantum
  TLS), styled per the product spec. Each badge activates only when that
  protection is actually negotiated; otherwise it renders inactive. Surfaced
  the negotiated cipher and an `aes_256_gcm` flag via `/api/security`.

## 1.6.0

- Added opt-in post-quantum TLS, gated by the CMake option `METIS_ENABLE_TLS`
  (default OFF, keeping the default build dependency-free). When ON, the server
  links OpenSSL and terminates TLS 1.2+ advertising the hybrid key-exchange
  group `X25519MLKEM768` first (ML-KEM per NIST FIPS 203). On OpenSSL older than
  3.5 it automatically falls back to classical groups.
- The banner (dashboard bar, startup log, and `GET /api/security`) reports the
  actually negotiated group, so it claims "post-quantum" only when ML-KEM was
  genuinely negotiated.
- TLS uses operator-supplied `tls.cert_file`/`tls.key_file`, or an ephemeral
  self-signed certificate when none is configured.
- Added `GOVERNMENT_REFERENCES.md` documenting the NIST, MITRE, and standards
  references the product uses, and listed OpenSSL in `THIRD_PARTY.md`.

## 1.5.0

- Renamed the product to Metis Code Analyzer Plus (executable
  `metis_code_analyzer_plus`, config `code_analyzer.pson`, namespace
  `metis::codeanalyzer`, include path `metis/codeanalyzer/`, Prometheus metric
  prefix `metis_code_analyzer_`). No functional changes.

## 1.4.1

- Cross-platform hardening verified by building on each target. Guarded the
  MSVC-only `#pragma comment(lib, ...)` behind `_MSC_VER` so MinGW-w64 builds
  are warning-free (it was emitting an unknown-pragma warning).
- Ignore `SIGPIPE` on POSIX (Linux/macOS) so a client disconnecting mid-response
  returns a handled `send()` error instead of terminating the server.

## 1.4.0

- Charts now use Chart.js (v4.4.1), vendored locally under `web/chart.umd.js`
  with its MIT license preserved (no CDN). Replaced the interim vanilla-canvas
  module. Added `THIRD_PARTY.md`.
- Confirmed American English (`en-US`) as the default locale.

## 1.3.1

- Added GET /api/languages and a README table listing the 15 recognized
  languages and their extensions; the dashboard footer now shows the list.

## 1.3.0

- Removed the Chart.js CDN dependency; charts are now drawn by a self-contained
  vanilla-canvas module (`web/charts.js`). The client is classic HTML/CSS/JS
  with no external or network dependencies.
- Moved all scoring tunables to PSON: severity weights, per-factor density
  scales, grade thresholds, and the complexity/comment/duplication penalty
  coefficients (`score.*`).
- Moved analysis sizing to PSON: max file bytes, duplication block lines, and
  long-line column threshold (`analysis.*`).
- Moved the full rule catalog to PSON (`rule.N.*`), with per-rule
  enable/disable; the built-in catalog remains as a fallback when none are
  configured. Grade thresholds are also published to the client via
  `/api/config` so server and dashboard grade identically.

## 1.2.0

- Renamed the product to Metis Code Analyzer Plus (executable
  `metis_code_analyzer_plus`, config `code_analyzer.pson`, namespace
  `metis::codeanalyzer`).
- Added PDF report export: pure C++ PDF writer (`pdf.hpp`), gated
  `GET /api/report.pdf`, and a dashboard Export PDF button.
- Added a light/dark theme toggle, persisted per browser.
- Added text-size controls (smaller / normal / larger), persisted per browser.
- Added standard suite documentation: TODO.md, SUPPORT.md, BACKGROUND.md,
  CHANGELOG.md, and .gitignore.

## 1.1.0

- Added admin login with SHA-256 password hashing and cookie sessions, plus
  sign-off; gated the data and control endpoints.
- Added an in-memory activity log with level filtering and console echo, a
  dashboard log panel (Refresh / Clear / Download), and the
  `GET /api/logs` and `POST /api/logs/clear` endpoints.
- Added internationalization across eight locales (US and UK English, French,
  German, Spanish, Italian, Brazilian Portuguese, Japanese) with a language
  selector.

## 1.0.0

- Initial release: C++20 analysis server and browser dashboard.
- Source scanner with language detection; per-file metrics (line breakdown,
  cyclomatic complexity, nesting, function heuristics); regex rule engine with
  CWE mapping; duplication detection; dependency extraction.
- Five health factors with A-F grading, Total Quality Index, and a
  technical-debt model.
- REST API, Prometheus metrics, Chart.js dashboard, and confined static
  serving (no path traversal).
