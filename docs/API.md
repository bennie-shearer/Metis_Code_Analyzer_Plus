# Metis Code Analyzer Plus - REST API

Version 2.5.1. All responses are JSON unless noted. The server listens on
`server.host` and `port` from `code_analyzer.pson` (default `0.0.0.0:8080`).

---

## Open Endpoints (no authentication required)

### GET /api/version

Product name, version, rule count, and active rule count.

```json
{ "product": "Metis Code Analyzer Plus", "version": "2.5.1",
  "rules": 20, "rules_active": 14 }
```

### GET /api/health

Liveness probe. Returns 200 when the server is running.

```json
{ "status": "ok" }
```

### GET /api/session

Reports whether the caller is currently authenticated.

```json
{ "authenticated": true, "auth_required": true, "username": "admin" }
```

### GET /api/languages

Languages recognized by the scanner with their file extensions.

```json
[ { "name": "C++", "extensions": [".cpp", ".cc", ".cxx"] }, ... ]
```

### GET /api/security

TLS and cipher status for the current connection.

```json
{
  "tls": true,
  "tls_configured": true,
  "protocol": "TLS 1.3",
  "cipher": "TLS_AES_256_GCM_SHA384",
  "group": "X25519MLKEM768",
  "post_quantum": true,
  "aes_256_gcm": true,
  "cert_cn": "Metis Code Analyzer Plus",
  "detail": ""
}
```

---

## Authentication

When `auth.enabled` is true in PSON, all data and control endpoints require a
valid session cookie (`mcsid`) or an `X-API-Key` header (when `api_key.enabled
= true`). Unauthenticated requests return HTTP 401.

### POST /api/login

Body: `{ "username": "...", "password": "..." }`.
Success sets an HttpOnly `mcsid` session cookie and returns `{ "ok": true }`.
Failure returns 401 with `{ "ok": false, "error": "invalid credentials" }`.

### POST /api/logout

Ends the session and clears the cookie. Returns `{ "ok": true }`.

### GET /api/hash-password?password=VALUE

Returns the SHA-256 hex digest of the supplied password for use as
`auth.password_sha256` in `code_analyzer.pson`.

```json
{ "sha256": "5e884898da28047151d0e56f8dc6292..." }
```

Also accepts `POST /api/hash-password` with body `{ "password": "..." }`.

---

## Configuration

### GET /api/config

Effective server configuration. Returned `ai` sub-object exposes non-secret
AI settings; the API key is never included.

```json
{
  "scan_root": ".",
  "static_dir": "web",
  "hourly_rate": 75.0,
  "dev_minutes_per_line": 0.5,
  "rules": 20,
  "auth_required": true,
  "default_locale": "en-US",
  "locales": ["en-US", "en-GB", "fr-FR", "de-DE", "es-ES", "it-IT", "pt-BR", "ja-JP"],
  "exclude": [".git", "build", "node_modules"],
  "max_table_rows": 500,
  "ai_enabled": false,
  "ai": {
    "enabled": false,
    "provider": "",
    "model": "",
    "max_output_tokens": 1024,
    "timeout_seconds": 30,
    "max_report_issues": 15
  },
  "grade_thresholds": { "a": 90, "b": 80, "c": 65, "d": 50 }
}
```

---

## Scanning

### GET /api/scan/ws (WebSocket) — v2.5.0

Live scan progress via WebSocket (RFC 6455). Upgrade with `wss://` (TLS) or
`ws://` (plain HTTP). Authentication via `mcsid` session cookie in the upgrade
request. Optional query parameter `?root=PATH` overrides the scan root.

Server emits JSON text frames:

```json
{"event":"start","root":"/path/to/source"}
{"event":"done","files":17,"issues":0,"tqi":98.8}
```

The dashboard Analyze button uses this endpoint automatically and falls back
to `POST /api/scan` if WebSocket is unavailable.

### POST /api/scan

Trigger a scan (HTTP fallback). Body: `{ "root": "/path/to/source" }` (optional;
defaults to current scan root). Returns the analysis report JSON on completion.

### GET /api/results

Latest analysis report: file count, line counts, complexity, issue count,
health factors, TQI, grade, debt, language breakdown, duplicates.

### GET /api/files

Per-file metrics: path, language, code/comment/blank lines, functions,
cyclomatic complexity, max nesting, issue count, dependencies.

### GET /api/issues

Issue list. Optional query parameters:
- `?severity=critical|major|minor|info` to filter.
- `?limit=N` to cap results.

Each issue: `rule_id`, `title`, `cwe`, `severity`, `factor`, `file`, `line`,
`snippet`, `remediation_minutes`, `status`, `note`.

### GET /api/history

Array of past scan snapshots (timestamp, TQI, issue count, file count).
Maximum count set by `ui.history_max` in PSON.

### GET /api/gate

Quality gate result based on `gate.*` PSON thresholds.

```json
{
  "enabled": true,
  "passed": true,
  "reason": "",
  "tqi": 94.9,
  "issues": 0,
  "critical": 0,
  "min_tqi": 70.0,
  "max_issues": 50,
  "max_critical": 0
}
```

### GET /api/source?path=FILE&line=N

Returns source lines around the given file and line for the inline preview.

```json
{ "file": "src/main.cpp", "line": 42, "lines": [ { "n": 40, "text": "..." }, ... ] }
```

---

## Issue Lifecycle

Issue status overrides persist across server restarts (v2.5.0). Status is
written to `data/issue_status.ndjson` (configurable via `ui.issue_status_file`
in `code_analyzer.pson`) on every `POST /api/issue/status` call and reloaded
at startup.

### GET /api/issue/status?key=RULE:FILE:LINE

Returns the current lifecycle status for an issue.

```json
{ "key": "SEC-EVAL:src/main.cpp:42", "status": "open", "note": "" }
```

Valid statuses: `open`, `accepted`, `wontfix`, `false_positive`.

### POST /api/issue/status

Set lifecycle status for an issue.

Body: `{ "key": "RULE:FILE:LINE", "status": "accepted", "note": "Reviewed by team lead." }`
Returns: `{ "ok": true }`

---

## Reports

### GET /api/report.pdf

Downloads a PDF quality report (grade, health factors, summary, top issues).
Filename derived from the scanned project name.

### GET /api/report.json

Full structured JSON export: report, per-file metrics, and all issues.

### GET /api/report.csv

Issues export as CSV with a provenance preamble. Filename derived from the
scanned project name.

---

## Technologies

### GET /api/technologies

Detected frameworks and libraries from import/include patterns (70+ patterns
across JavaScript, Python, Java, Go, Rust, C++). Requires
`technologies.enabled = true` in PSON.

```json
[ { "name": "React", "category": "JavaScript", "file_count": 12 }, ... ]
```

---

## Licenses

### GET /api/licenses

License header detection results per file.

---

## Projects

### GET /api/projects

Named project list when `project.N.name` / `project.N.root` keys are set.
Returns `[]` when using the implicit single-project mode.

### POST /api/project/select

Switch the active project. Body: `{ "name": "project-name" }`.

---

## Browser / Directory

### GET /api/browse?path=DIR

Lists immediate subdirectories of DIR for the scan-root browser.
On Windows also returns `{ "drives": ["C:/", "D:/", ...] }` at root.

---

## Infrastructure Stubs (planned C++20)

### GET /api/gpu

Returns GPU subsystem status. Currently always `"planned"`.

```json
{ "subsystem": "gpu", "enabled": false, "status": "planned",
  "note": "GPU acceleration is planned (C++20; Windows/Linux/macOS)." }
```

### GET /api/kubernetes

Returns Kubernetes integration status. Currently always `"planned"`.

### GET /api/containers

Returns Container integration status. Currently always `"planned"`.

---

## Logging

### GET /api/logs?limit=N

Recent log entries (default limit: 200), most-recent last.

```json
[ { "timestamp": "2026-01-01T12:00:00Z", "level": "INFO", "message": "..." } ]
```

### POST /api/logs/clear

Clears the in-memory log buffer. Returns `{ "ok": true, "cleared": N }`.

---

## AI

### POST /api/ai/summarize

Requests an AI summary of the latest analysis report. Requires a TLS-enabled
build and `ai.enabled = true` in PSON. Calls the configured OpenAI-compatible
provider over HTTPS.

```json
{ "ok": true, "response": "The scan found 3 security issues..." }
```

On error: `{ "ok": false, "error": "...", "status": 500 }`

---

## Metrics

### GET /metrics

Prometheus text-format metrics: TQI, issue counts by severity, health factors,
file and function counts, debt minutes.

---

## Operations

### POST /api/shutdown

Graceful shutdown. Requires authentication. Returns
`{ "status": "shutting down" }` and stops the server after ~100 ms.

---

## Static Dashboard

`GET /` serves `index.html` from the configured `server.static_dir`.
All other unmatched `GET` paths serve files from that directory. Paths are
confined to the static root; directory traversal returns 404.

`GET /docs/openapi.yaml` serves the machine-readable OpenAPI 3.0 spec
(no authentication required).
