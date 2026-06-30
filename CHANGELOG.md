# Changelog - Metis Code Analyzer Plus

See `docs/CHANGELOG.md` for the full version history.

## v2.7.14 (2026-06-29)

ROOT CAUSE FOUND AND FIXED: pson.hpp's load()/load_string() read the config
file strictly line by line, with no support for array values that span
multiple lines. scan.exclude in the shipped code_analyzer.pson is written
across 5 lines for readability. As a result, only the FIRST line was ever
parsed into scan.exclude (".git", "build", "node_modules", with a dangling
unclosed bracket and trailing comma) -- every entry on lines 2-5, including
"vendor", "third_party", "openssl-prebuilt", "certs", "sqlite", and "tests",
was silently dropped and never excluded from scanning.

This explains the unexpectedly low Transferability (74) and Changeability
(72) health scores and the 75,165 duplicate code blocks reported: the
tree-sitter vendored grammar parsers under vendor/tree-sitter-grammars/
(near-identical machine-generated parser.c/scanner.c files for go, java,
python, rust, c, and javascript) were being fully scanned and duplicate-
matched against each other, despite "vendor" being correctly listed in
scan.exclude.

Fix: pson.hpp now detects an array literal with an unmatched '[' (outside
quoted strings) and transparently continues reading and appending subsequent
lines until the bracket closes, before handing the assembled line to the
existing key=value parser. Verified via standalone test: all 14 entries in
the shipped scan.exclude array now parse correctly, and the line immediately
following the multi-line array (scan.watch = false) still parses correctly
as well.

Also tightened the scan.exclude diagnostic log added in v2.7.13 to wrap each
entry in quotes and show the total count, e.g.:
"Scan excludes active (14): ".git", "build", "node_modules", "vendor", ..."

After upgrading, expect the duplicate-block count and the Transferability /
Changeability scores to change significantly once vendor/ is correctly
excluded from analysis.

---

## v2.7.13 (2026-06-29)

Bug fix and diagnostics:

1. rules.hpp: ROB-TODO's own pattern definition string ("\b(todo|fixme|hack|xxx)\b")
   contains the literal words it searches for, causing it to self-match if
   rules.hpp is ever scanned. Added a metis-suppress ROB-TODO inline comment
   on the rule definition line to prevent this false positive. Confirmed via
   audit that no actual TODO/FIXME/HACK markers exist anywhere else in the
   shipped source tree. (code_analyzer.pson was also checked: .pson is not a
   registered scanned-language extension, so it is never analyzed and needed
   no suppression.)

2. main.cpp: Added startup diagnostic logging of the active scan.exclude list
   ("Scan excludes active: ...") so the effective exclude configuration is
   always visible in the Activity Log at boot. Also added duplicate-block
   counts to all three scan-completion log messages (initial startup scan,
   POST /api/scan, and the WebSocket scan handler) to make it easy to spot
   unusually high duplication (e.g. from vendored/generated code such as
   tree-sitter grammar parsers) without needing to open the dashboard.

---

## v2.7.12 (2026-06-29)

Cleanup release: removed all diagnostic console.log/console.warn instrumentation
added in v2.7.10 for tracing the Analyze button spinner issue, now confirmed
fixed in v2.7.11. The hardened reset logic and 20-second safety timeout are
retained as permanent defensive measures, just without the verbose tracing.
No functional changes from v2.7.11.

---

## v2.7.11 (2026-06-29)

Two real bugs found and fixed via debug trace analysis:

1. style.css: .scan-progress used display:inline-flex, which has higher CSS
   specificity than the browser's default [hidden] { display: none } rule.
   Setting prog.hidden = true in JavaScript correctly set the hidden attribute,
   but the element remained visually displayed because the inline-flex display
   value was never overridden. This is the actual root cause of the Analyze
   button's spinner/"Scanning..." indicator appearing to stay stuck even though
   the JS reset logic was running correctly the entire time. Added an explicit
   .scan-progress[hidden] { display: none !important; } rule, matching the
   pattern already used for .login-overlay[hidden] and .browse-overlay[hidden].

2. main.cpp: the WebSocket "start" event message was built by direct string
   concatenation of the raw scan root path into a JSON literal, with no
   escaping. Windows paths contain backslashes (e.g. C:\Users\...), which
   produced invalid JSON escape sequences (\C, \U, etc.), causing
   JSON.parse() to throw "Bad escaped character" in the browser console on
   every scan. Both the "start" and "done" WebSocket messages now use the
   existing mc::json::Value builder (already used throughout the REST API)
   to guarantee correct escaping.

---

## v2.7.10 (2026-06-29)

Diagnostic build: added console.log/console.warn debug messages throughout
the Analyze button's WebSocket scan handler (ws.onopen, ws.onmessage,
ws.onerror, ws.onclose, fallbackScan, resetUI) to trace exactly which path
is taken and where the UI reset succeeds or fails. Also added a hard 20-second
safety timeout that unconditionally resets the button/spinner regardless of
WebSocket behavior, guaranteeing the Analyze button can never remain stuck
even if the cause of the original spinner issue is not yet fully diagnosed.

Open browser DevTools Console before clicking Analyze to capture the trace.

---

## v2.7.9 (2026-06-29)

Bug fix: Infrastructure panel (GPU, Kubernetes, Containers) no longer shows
"LOADING..." permanently. The API calls were firing concurrently with loadAll()
and the page settle cycle was resetting the span text before the async responses
arrived. Moved infra status fetches into a loadInfra() function that runs after
loadAll() resolves, guaranteeing the spans are updated last and stay visible.

---

## v2.7.8 (2026-06-29)

Bug fix: Infrastructure panel (GPU, Kubernetes, Containers) no longer shows
"LOADING..." permanently. Two issues fixed:

1. app.js: The catch handler was silently swallowing errors, leaving the badge
   text at the initial "loading..." value when the API call failed. The catch
   now sets the badge to "unavailable" so failures are visible.
2. app.js: The "disabled" status was not mapped to a CSS class, so disabled
   services fell through without any visual styling change. Class list is now
   explicitly managed: planned/enabled/disabled each get their own class.
3. style.css: Added .infra-badge.disabled style (muted grey) to complement
   the existing .infra-badge.planned and .infra-badge.enabled styles.

---

## v2.7.7 (2026-06-29)

Bug fix: Analyze button now always stops spinning after a WebSocket scan
completes. Previously the button/spinner reset relied solely on a 1800ms
setTimeout inside the "done" message handler. If the WebSocket close event
fired before or instead of the done message being processed, the button
stayed disabled and the spinner kept running indefinitely even though results
had loaded. The ws.onclose and ws.onerror handlers now unconditionally reset
the button, label, and progress bar whenever the connection was successfully
opened (wsOk = true), guaranteeing cleanup regardless of message ordering.

---

## v2.7.6 (2026-06-29)

Bug fix: Analyze button spinner no longer persists after scan completion.
The 1800 ms post-scan timeout in app.js was resetting the progress label
back to "Scanning..." instead of clearing it, making the header appear to
keep spinning even after results had loaded. Label is now cleared to "".

---

## v2.7.5 (2026-06-29)

Bug fix: WebSocket scan handler now URL-decodes the `root` query parameter
before scanning and logging. Previously the raw percent-encoded path
(e.g. C%3A%2FUsers%2F...) was passed directly to the analyzer, which found
0 files and left the Analyze button spinning indefinitely with "Scanning..."
shown in the header. The `url_decode()` helper already existed and was used
correctly in all other handlers (/api/browse, /api/source, /api/hash-password)
but had been left as a TODO placeholder in the WebSocket handler since v2.5.0.

---

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

Default port changed from 8080 to 8443. Chrome treats port 8080 as HTTP
and silently downgrades https:// to http:// for localhost. Port 8443 is
the conventional HTTPS alternate port and Chrome handles it correctly.
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

Build fix: HTTP-to-HTTPS redirect block in `http.hpp` had bare newlines
inside string literals (introduced by Python heredoc in v2.6.6), causing
"missing terminating quote" compiler errors. Block rewritten with correct
escape sequences and per-statement `resp +=` construction.

---

## v2.6.6 (2026-06-28)

Two fixes:

1. HTTP-to-HTTPS redirect: when TLS is enabled and a browser connects with
   plain http://, the server now peeks at the first byte, detects the non-TLS
   connection, and sends a 301 redirect to https:// instead of dropping the
   connection silently (which the browser showed as "Not Secure").

2. SEC-PATH-TRAVERSAL false positive: tightened rule.29.pattern to require ..
   to be preceded by a word character, slash, or quote. Suppresses false
   positives on Windows named-pipe paths (\\.\\ in C++ source).

---

## v2.6.5 (2026-06-28)

Restored `scan.root`, `tls.cert_file`, and `tls.key_file` in
`code_analyzer.pson` to their v2.5.3 values. These were cleared in v2.6.0
when removing hardcoded paths, but they are required deployment values, not
hardcoded parameters -- the PSON is the correct place for them.

---

## v2.6.4 (2026-06-28)

Bug fix: export filenames (PDF, JSON, CSV) now always derive from the actual
scanned directory name. Project roots are resolved to absolute paths at startup
via `std::filesystem::weakly_canonical`, before being stored in `ProjectState`.
`download_stem()` reverted to its original simple implementation -- path
resolution now happens at the correct point (startup) rather than at export
time.

---

## v2.6.3 (2026-06-28)

Bug fix: replaced "Metis CAST Plus" with "Metis Code Analyzer Plus" in the
shutdown confirmation dialog across all eight locales in `web/i18n.js`.

---

## v2.6.2 (2026-06-28)

Bug fix: `/api/gpu`, `/api/kubernetes`, and `/api/containers` were called in
`DOMContentLoaded` before the session check, producing 401 console errors on
every page load when `auth.enabled = true`. Moved all three into the
`checkSession().then()` post-auth block alongside `/api/projects`.

---

## v2.6.1 (2026-06-28)

Bug fix: `download_stem()` now resolves relative paths (including `.`) to
an absolute path via `std::filesystem::weakly_canonical` before extracting
the base name. Previously `scan.root = "."` produced `report-issues.csv`
instead of a name derived from the actual working directory.


See `docs/CHANGELOG.md` for the full version history.

## v2.6.0 (2026-06-28)

Full PSON documentation; BACKGROUND.md rewritten; README.md cleaned for
GitHub; LICENSE updated; .gitignore created; all Unicode replaced with ASCII;
hardcoded paths removed from PSON defaults; all version references verified
at 2.6.0. See `docs/CHANGELOG.md` for complete details.
