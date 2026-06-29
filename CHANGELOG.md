# Changelog - Metis Code Analyzer Plus

See `docs/CHANGELOG.md` for the full version history.

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
