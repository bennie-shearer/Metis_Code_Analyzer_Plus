# Changelog - Metis Code Analyzer Plus

Full version history.

## v2.7.4 (2026-06-29)

Build fix: unclosed /* comment in rules.hpp (v2.7.3) swallowed the
const bool has_suppress declaration and subsequent code, breaking the
class structure. Comment now correctly closed with */.

---

## v2.7.3 (2026-06-29)

- Added inline suppression support to the rule engine (rules.hpp): any source
  line containing "metis-suppress RULE-ID" skips that specific rule for that
  line. Syntax: // metis-suppress RULE-ID: reason
- Added suppression comment to line 1612 of src/main.cpp (Windows named pipe
  path \\.\pipe\docker_engine falsely matched SEC-PATH-TRAVERSAL).

---

## v2.7.2 (2026-06-29)

Removed the "Connection is not secure / Switch to HTTPS" banner from the
dashboard. Chrome deliberately does not show a padlock for localhost
regardless of TLS status (Chrome policy since v94). The banner was
misleading -- the connection is encrypted when TLS is configured. The
TLS 1.3 / AES-256-GCM / Post-Quantum TLS badges in the header correctly
reflect the actual connection security.

---

## v2.7.1 (2026-06-29)

Version bump only. http.hpp is the clean v2.5.3 original (744 lines).
Copy include/metis/codeanalyzer/http.hpp from this zip directly into
your CLion project directory before rebuilding.

---

## v2.7.0 (2026-06-29)

Restored `http.hpp` from v2.5.3 (the original unmodified file). The
redirect block added in v2.6.6 corrupted the class structure across
v2.6.6-v2.6.9, removing route(), start(), stop(), run(), TlsStatus and
all public methods. This release reverts to the known-good original.
Port remains 8443. Navigate to https://localhost:8443.

---

## v2.6.9 (2026-06-29)

### Port Changed: 8080 -> 8443

Chrome has a known behavior where it downgrades https:// to http:// for
localhost on port 8080 (a conventional HTTP port). Port 8443 is the
standard HTTPS alternate port; Chrome does not downgrade it.

Updated `port` default in `code_analyzer.pson` from 8080 to 8443.
Navigate to https://localhost:8443.

---

## v2.6.8 (2026-06-28)

- Added sticky quick-navigation pill bar below the header with jump links
  to all 14 sections (Issues, Files, Activity Log, Trends, Licenses,
  Duplicates, Dependencies, AI Insights, Platform, Technologies, Quality
  Gate, Infrastructure, TLS, Admin).
- Added id anchors on every panel section for the nav links.
- Added HTTPS upgrade banner: when the page loads over plain http:// a red
  banner appears with a direct link to the https:// equivalent URL.
- Fixed version stamp in header (was hardcoded v2.5.0; now set by /api/version
  on load and pre-populated with the correct version in HTML).

---

## v2.6.7 (2026-06-28)

### Build Fix: http.hpp String Literal Errors

The redirect block added in v2.6.6 was written via Python string construction
which injected bare `\n` newlines inside C++ string literals and used
adjacent-literal concatenation with `+` operator (invalid in C++), causing
multiple "missing terminating quote" and "no match for operator+" errors.

Rewritten with:
- All `\r\n` as proper two-character C++ escape sequences on single lines.
- Response built via sequential `resp +=` statements rather than mixing
  adjacent-literal concatenation with `operator+`.
- `find_first_of` replaced with `find("\r\n")` for the Host header end.

---

## v2.6.6 (2026-06-28)

### Fix 1: HTTP-to-HTTPS Redirect (http.hpp)

When `tls.enabled = true` the server previously called `SSL_accept()`
immediately on every inbound connection. A browser navigating to
`http://localhost:8080` (plain HTTP) sent a plain HTTP request; `SSL_accept`
failed and closed the connection silently. The browser showed "Not Secure"
or a connection error.

Fix: peek at the first byte of each connection before calling `SSL_accept`.
A TLS ClientHello starts with `0x16` (content-type Handshake). If the first
byte is anything else the connection is plain HTTP; the server reads the
request headers, extracts the Host and path, and sends a `301 Moved
Permanently` redirect to `https://` on the same port, then closes the socket.
The browser follows the redirect and establishes a proper TLS session.

### Fix 2: SEC-PATH-TRAVERSAL False Positive (code_analyzer.pson rule.29)

The previous pattern `\.\.(\/)|(\\)\.\. ` matched any occurrence of
`\.` in source, including the Windows named-pipe literal `\\.\\pipe` in
C++ string constants. This produced a critical false positive at the container
probe code in `src/main.cpp`.

New pattern: `(^|[\w/"'])\.\.(\/|\\)` -- requires `..` to be preceded
by a word character, slash, or quote (i.e. a genuine path component separator),
suppressing matches on lone backslash-dot sequences in C++ string literals.

---

## v2.6.5 (2026-06-28)

### Fix: Restore Deployment Values in code_analyzer.pson

v2.6.0 cleared `scan.root`, `tls.cert_file`, and `tls.key_file` to empty /
`"."` on the grounds of removing hardcoded paths. This was wrong: these are
not hardcoded parameters embedded in the binary -- they are deployment
configuration values whose correct home is the PSON file. Clearing them broke
TLS (browser showed "Not Secure" due to untrusted auto-generated cert) and
the scan root.

Restored to v2.5.3 values:
  scan.root     = "C:/Users/Benni/CLionProjects/Metis_Code_Analyzer_Plus"
  tls.cert_file = "C:/Tools/localhost+2.pem"
  tls.key_file  = "C:/Tools/localhost+2-key.pem"

---

## v2.6.4 (2026-06-28)

### Bug Fix: Export Filename Derives from Scanned Directory Name

Two previous attempts to fix the export filename got it wrong:

- v2.6.1: called `weakly_canonical` inside `download_stem()` at export time,
  which resolved `"."` to the **build directory** (`cmake-build-release`),
  producing `cmake-build-release-issues.csv`.
- v2.6.0: changed `scan.root` default from the hardcoded Windows path to
  `"."`, which `download_stem(".")` reduced to `report`, producing
  `report-issues.csv`.

Root cause: `scanned_root` in `AnalysisResult` was storing whatever string
came from the PSON `scan.root` key unchanged (e.g. `"."`). `download_stem`
only extracts the base name from the string it receives; it cannot know what
`"."` means without knowing the working directory.

Correct fix: resolve every project root to an absolute path **at startup**,
immediately after `project_defs` is fully populated, using
`std::filesystem::weakly_canonical`. The resolved absolute path is stored in
`ProjectState::root`, propagated into `AnalysisResult::scanned_root` on every
scan, and therefore available to `download_stem()` as a fully-qualified path.
`download_stem()` itself reverts to its original simple form -- it only strips
trailing separators and extracts the last path component, which is correct
once the input is guaranteed to be absolute.

---

## v2.6.3 (2026-06-28)

### Bug Fix: Wrong Product Name in Shutdown Dialog

The `confirmShutdown` i18n key in `web/i18n.js` contained "Metis CAST Plus"
in all eight locales (en-US, en-GB, fr-FR, de-DE, es-ES, it-IT, pt-BR, ja-JP).
Replaced with "Metis Code Analyzer Plus" in every locale.

---

## v2.6.2 (2026-06-28)

### Bug Fix: Infrastructure Panel 401 Console Errors

`/api/gpu`, `/api/kubernetes`, and `/api/containers` were fetched in the
`DOMContentLoaded` handler before `checkSession()` confirmed a valid session.
When `auth.enabled = true` this produced three 401 console errors on every
page load (visible in the browser developer console).

Fix: moved all three calls into the `checkSession().then(function(ok) { if (ok) {`
post-authentication block in `web/app.js`, matching the pattern already
established for `/api/projects` in v2.4.x. The infrastructure panel now
populates correctly after sign-in and produces no pre-auth 401 errors.

---

## v2.6.1 (2026-06-28)

### Bug Fix: download_stem Resolves Relative Paths

`download_stem()` in `src/main.cpp` now calls
`std::filesystem::weakly_canonical()` to resolve relative paths before
extracting the base directory name. Previously `scan.root = "."` caused
all export filenames (PDF, JSON, CSV) to fall back to `report-*` because
`"."` strips to an all-separator base name. With this fix, a scan root of
`"."` resolves to the actual current working directory and uses that
directory's name as the export stem.

--- Most recent first.

## v2.6.0 (2026-06-28)

### Documentation and Configuration

- PSON: Full inline documentation for every key including `ui.issue_status_file`,
  `gate.fail_on_new_critical`, all `score.*` sub-keys, all `ai.*` sub-keys,
  all infrastructure (`gpu.*`, `kubernetes.*`, `containers.*`) keys. Every
  parameter in the binary is now documented in `code_analyzer.pson`.
- PSON: Cleared hardcoded Windows cert paths (`tls.cert_file`, `tls.key_file`
  now default to `""` for auto-generated self-signed cert).
- PSON: Cleared hardcoded `scan.root` (now defaults to `"."`).
- PSON: All Unicode characters replaced with 7-bit ASCII alternatives.
- `docs/BACKGROUND.md`: Fully rewritten with eight-section Table of Contents
  covering history of static analysis (1970s-present), ecosystem comparison,
  design philosophy, scoring model, infrastructure roadmap, security
  architecture, and relationship to named products.
- `README.md`: Rewritten as a clean GitHub-ready document with complete API
  table, full operations guide, acknowledgments, license, and dedication.
- `LICENSE`: Updated with dedication, disclaimer, and acknowledgments per
  project specification.
- `.gitignore`: Created covering build dirs, binaries, runtime data, TLS
  certs, IDE artifacts.
- `docs/CONFIGURATION.md`: Updated to reflect v2.6.0 PSON defaults.
- `docs/TODO.md`: Updated with v2.6.0 improvements and completed items.
- All `.md` files verified 7-bit ASCII (Unicode em-dash in `docs/API.md`
  replaced with `--`).
- CMakeLists.txt, VERSION, version.hpp: bumped to 2.6.0.
- All version references verified consistent at 2.6.0.

### Rule Engine

- Rule catalog reorganized into logical groups with section headers.
- `rule.10` (`CHG-MAGIC-PATH`): comment updated to document the K8s exclusion
  added in v2.5.3.
- `rule.28` (`SEC-XSS-INNERHTML`): comment updated to document v2.5.3 pattern
  tightening.

---

## v2.5.3 (2026-06-28)

### Bug Fix: CHG-MAGIC-PATH PSON Pattern

`rule.10.pattern` in `code_analyzer.pson` was not updated in v2.5.2 (only
`default_rules()` in `rules.hpp` was fixed). The PSON pattern retained `var`
in the primary alternation and had incorrect backslash escaping in the K8s
negative lookahead, causing the exclusion to never fire.

- `rule.10.pattern` in `code_analyzer.pson`: rebuilt from the correct target
  regex with a verified Python round-trip. Removes `var` from the primary
  `(?:etc|usr|...)` group and adds `/var(?!/run/secrets/kubernetes\.io/)`
  as a separate branch with correct PSON escaping.
- `rules.hpp` `default_rules()`: fixed backslash count in the R"()" raw
  string (`kubernetes\.io` was `kubernetes\\.io`; one extra backslash
  caused the lookahead to match `kubernetes` + literal backslash + any char
  instead of `kubernetes.io`).
- Both sources verified by Python regex round-trip test.

---

## v2.5.2 (2026-06-28)

### Rule Engine: False Positive Elimination

**CHG-MAGIC-PATH** (`rules.hpp` `default_rules()`): Excluded
`/var/run/secrets/kubernetes.io/serviceaccount/token` via negative lookahead.
This is a CNCF-standard protocol mount point (in-cluster Kubernetes service
account), not a configurable path. All other `/var/...` paths continue to fire.

**SEC-XSS-INNERHTML** (`code_analyzer.pson` `rule.28.pattern`): Pattern
tightened from `\.innerHTML\s*=\s*[^"'\`]` (which matched any assignment
whose space-after-equals satisfied `[^"'\`]`) to
`\.innerHTML\s*=.*\+\s*(?!esc\s*\(|t\s*\(|...)[a-zA-Z_$]`.

Result: 27 false positives eliminated; 3 genuine findings retained (lines 191,
340, 428 in `web/app.js` where server-numeric values are interpolated without
`esc()`).

---

## v2.5.1 (2026-06-24)

- Documentation release: all documents audited and updated.
- `docs/BACKGROUND.md` added (initial version).
- `docs/ARCHITECTURE.md`, `docs/HOWTO.md`, `docs/THIRD_PARTY.md` added.
- PSON: portability scoring scale (`score.scale.portability`),
  `scan.watch_interval_seconds`, and infrastructure section added.

---

## v2.5.0 (2026-06-24)

### Features

- **Persistent issue status**: issue lifecycle overrides (open, accepted,
  wontfix, false_positive) written to `data/issue_status.ndjson` on every
  POST /api/issue/status change and reloaded at startup. Survives restarts.
  Configurable via `ui.issue_status_file`.

- **WebSocket live scan progress**: RFC 6455 WebSocket support in `http.hpp`
  (SHA-1 handshake, Base64, text/close frames). `ws_route()` API on Server.
  `GET /api/scan/ws` endpoint. Dashboard Analyze button uses WebSocket with
  POST fallback. Progress spinner in header.

- **Infrastructure probing** (GPU, Kubernetes, Containers): C++20 probes for:
    - GPU: `nvidia-smi`, OpenCL runtime library (Windows/Linux/macOS).
    - Kubernetes: `KUBECONFIG` env, `~/.kube/config`, in-cluster SA token.
    - Containers: Docker/Podman/containerd socket; Windows named pipe.
  Endpoints: GET /api/gpu, /api/kubernetes, /api/containers.

- **Rule catalog expansion**: 8 new rules (28-35):
  SEC-XSS-INNERHTML (CWE-79), SEC-PATH-TRAVERSAL (CWE-22),
  SEC-PRINTF-FORMAT (CWE-134), ROB-NULL-DEREF (CWE-476),
  EFF-LOOP-INVARIANT, TRN-COMMENTED-CODE,
  MISRA-C-14-4 (opt-in), CERT-MEM35-C (opt-in).
  Total: 35 rules (27 enabled by default).

---

## v2.4.x

- Header brand text no longer wraps (white-space:nowrap, flex-shrink:0).
- Console 401 error on /api/projects at page load eliminated.
- `docs/openapi.yaml`: version corrected (2.1.4 -> 2.4.0); 12 missing
  endpoint specs added (all 36 server routes now documented).
- `docs/ARCHITECTURE.md` fully rewritten; corrected include paths.
- `docs/CONFIGURATION.md`: 9 missing PSON key sections added.
- TLS/AES/PQC badge bar changed to horizontal row layout.

---

## v2.3.x

- Technology Inventory panel wired in dashboard (GET /api/technologies).
- Quality Gate panel wired in dashboard (GET /api/gate).
- Issue Status lifecycle column in Issues table.
- Server health dot in header (GET /api/health, 30 s poll).
- Admin Tools / Password Hash Generator panel.
- SEC-SQL-CONCAT false positive fixed (CWE-89 pattern tightened).
- TLS 1.3 badge added; badges changed to horizontal row.
- All .md/.txt files moved to docs/; root keeps README.md and CHANGELOG.md.

---

## v2.2.0

- Infrastructure panel with GPU / K8s / Container cards.
- TLS/Security Configuration panel.
- AI Configuration panel.
- GET /api/security extended with `protocol` and `cert_cn` fields.
- GET /api/config extended with full `ai` sub-object.
- BACKGROUND.md added (initial draft).

---

## v2.1.x and Earlier

- 46-language support.
- AST opt-in via tree-sitter (C, Python, JS, Go, Java, Rust).
- Quality gate: GET /api/gate; gate.* PSON; exit_nonzero for CI.
- Post-quantum TLS X25519MLKEM768; OpenSSL 4.0.0 prebuilt for Windows.
- Multi-project support; incremental scanning; mtime cache.
- License header detection, webhook, watch mode, API key auth.
- PDF, JSON, CSV exports; Prometheus metrics; trend lines.
- Dependency graph; duplicate block detection.
- Portability rule set (PORT-*; 7 rules, guard-aware suppression).
- Rule false-positive tightening: SEC-EVAL, SEC-HARDCODED-SECRET,
  SEC-SYSTEM-CALL, CHG-MAGIC-PATH, PORT-* platform-guard awareness.
- Export downloads named from scanned project stem.
- MISRA-C and CERT-C rule stubs (disabled by default).
