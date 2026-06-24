# Metis Code Analyzer Plus -- How-To Guide

A practical reference for installation, configuration, and daily use.

---

## 1. Building

### Requirements

| Requirement | Version | Notes |
|---|---|---|
| CLion | 2025.x or later | CMake configured automatically |
| MinGW-w64 GCC | 13.x | Bundled with CLion on Windows |
| CMake | 3.20 or later | Bundled with CLion |
| Ninja | any | Bundled with CLion |
| OpenSSL | 4.0.0 (prebuilt) | Included in openssl-prebuilt/windows/ |

### CMake Options

| Option | Default | Effect |
|---|---|---|
| `METIS_ENABLE_TLS` | ON | Build with TLS/HTTPS and post-quantum support |
| `METIS_ENABLE_AST` | OFF | Build with tree-sitter AST analysis for 6 languages |

To change an option in CLion: File -> Settings -> Build -> CMake ->
cmake-build-release -> CMake options. Example:
```
-DMETIS_ENABLE_TLS=ON -DMETIS_ENABLE_AST=ON
```

### First Build

1. Open the project in CLion.
2. Select the cmake-build-release configuration.
3. Build -> Rebuild Project.
4. Output: cmake-build-release/metis_code_analyzer_plus.exe (Windows)

---

## 2. TLS and HTTPS Setup

The server runs on HTTPS by default (tls.enabled = true in code_analyzer.pson).
For a green padlock in Edge and Chrome, generate a trusted certificate once.

### Generating the Certificate (Windows, one time)

Run in an Administrator PowerShell:
```powershell
powershell -ExecutionPolicy Bypass -File "C:\path\to\project\certs\gen_certs.ps1"
```

This creates certs/server.crt and certs/server.key in the project source tree,
and installs the certificate into Cert:\LocalMachine\Root so browsers trust it.

After running: Rebuild in CLion (CMake POST_BUILD copies certs/ to the build
directory). Restart the server. Kill and reopen the browser.

### Using mkcert (alternative)

If you already have mkcert installed:
```
mkcert localhost 127.0.0.1
```
This creates localhost+2.pem and localhost+2-key.pem in the current directory.
Point the PSON at those files:
```
tls.cert_file = "C:/Tools/localhost+2.pem"
tls.key_file  = "C:/Tools/localhost+2-key.pem"
```

### Disabling TLS

To run on plain HTTP, set in code_analyzer.pson:
```
tls.enabled = false
```
Access via http://localhost:8080 (no certificate required).

---

## 3. Authentication

Default credentials: admin / Admin#2026

Change the password before deployment. Generate a new hash:
```
POST https://localhost:8080/api/hash-password
Body: {"value": "your-new-password"}
```
Or with a browser-safe URL (encode # as %23):
```
GET https://localhost:8080/api/hash-password?value=YourPassword (no # in password)
```
Set the returned hash in code_analyzer.pson:
```
auth.password_sha256 = "<64-char hex hash>"
```

### API Key Authentication

For headless callers (CI pipelines, scripts):
```
auth.api_key_enabled = true
auth.api_key_sha256  = "<SHA-256 of your key>"
```
Include in requests:
```
X-API-Key: your-api-key
```

---

## 4. Configuring What to Scan

### Single directory
```
scan.root = "C:/path/to/your/project"
```

### Multiple projects
```
project.1.name = "Server"
project.1.root = "C:/projects/myapp/server"
project.2.name = "Client"
project.2.root = "C:/projects/myapp/client"
```
Switch projects via the dashboard dropdown or POST /api/project/select.

### Excluding paths
```
scan.excludes = ["vendor", "node_modules", "build", ".git", "dist"]
```

### Watch mode (auto-rescan on file change)
```
scan.watch                  = true
scan.watch_interval_seconds = 30
```

---

## 5. Running a Scan

Click Analyze in the dashboard toolbar, or:
```
POST https://localhost:8080/api/scan
```
With optional body: {"root": "/path/to/scan"}

The activity log (bottom of dashboard) shows scan progress.
Results update automatically when the scan completes.

---

## 6. Understanding Results

### Grade and TQI

Total Quality Index (TQI) is a 0-100 composite score weighted across five
health factors. Grade thresholds:

| Grade | TQI |
|---|---|
| A | >= 90 |
| B | 80-89 |
| C | 65-79 |
| D | 50-64 |
| F | < 50 |

### Health Factors

| Factor | What it measures |
|---|---|
| Robustness | Null checks, error handling, defensive patterns |
| Security | Hardcoded secrets, unsafe functions, exposed keys |
| Efficiency | Function length, duplicate code, complexity |
| Transferability | Comment ratio, naming conventions |
| Changeability | Cyclomatic complexity, long lines, magic numbers |

### Complexity

Average Complexity is the mean cyclomatic complexity per function (not per file).
Threshold is configurable per language in PSON:
```
score.complexity.threshold = 5.0
score.complexity.threshold.Python = 8.0
score.complexity.threshold.COBOL  = 15.0
```

### Issues

Each issue shows: severity, rule ID, finding description, CWE, file, line,
and estimated remediation time in minutes.

Severities: Critical, Major, Minor, Info.

Click any issue row to see an inline source preview.

---

## 7. Rule Configuration

### Enabling/disabling individual rules

Rules 1-14 are active by default. Rules 15-20 (MISRA-C / CERT-C) are disabled.
To enable a rule, set enabled = true in code_analyzer.pson:
```
rule.16.enabled = true   # MISRA-C-15-1: goto statement
rule.19.enabled = true   # CERT-STR31-C: unbounded string copy
```

### Issue lifecycle

Mark an issue as a false positive, accepted risk, or will-not-fix:
```
POST /api/issue/status
{"key": "RULE_ID:filepath:line", "status": "false_positive", "note": "..."}
```
Valid statuses: open, accepted, wontfix, false_positive.
Status is preserved in the issues list and shown per-issue.

---

## 8. Security Badges

Two badges appear when TLS is active and conditions are met:

| Badge | Color | Condition |
|---|---|---|
| AES-256-GCM | Blue | TLS_AES_256_GCM_SHA384 cipher negotiated |
| Post-Quantum TLS | Purple | X25519MLKEM768 key exchange negotiated |

The Post-Quantum badge requires OpenSSL 4.0+ (bundled for Windows) and a
browser that supports X25519MLKEM768 (Edge and Chrome both do as of 2025).

---

## 9. Quality Gate (CI/CD)

Enable the gate in code_analyzer.pson:
```
gate.enabled      = true
gate.min_tqi      = 80.0
gate.max_critical = 0
gate.exit_nonzero = true
```

Check gate status:
```
GET /api/gate
```
Response: {"passed": true/false, "tqi": 94.9, "critical": 0, ...}

With exit_nonzero = true, the server exits with code 1 on gate failure.
See docs/CI.md for GitHub Actions, Jenkins, and GitLab CI examples.

---

## 10. Exporting Results

| Format | Endpoint | Dashboard button |
|---|---|---|
| PDF | GET /api/report.pdf | Export PDF |
| JSON | GET /api/report.json | Export JSON |
| CSV | GET /api/report.csv | Export CSV |

PDF includes the health dashboard, issue table (up to ui.pdf_max_issues),
and file complexity chart.

---

## 11. Technology Inventory

The server detects which frameworks and libraries a project uses based on
import/include patterns. Covers 70+ technologies across JavaScript, Python,
Java, Go, Rust, and C++.

```
GET /api/technologies
```
Returns: [{"name": "React", "category": "Frontend framework", "file_count": 12}, ...]

Enable/disable detection:
```
technologies.enabled = true
```

---

## 12. Webhook

Fire an outbound POST after every scan:
```
webhook.enabled      = true
webhook.url          = "https://your-server/hook"
webhook.secret       = "your-secret"
webhook.min_grade    = "B"
webhook.on_regression = true
```
Payload: JSON with grade, TQI, issue count, and scan timestamp.

---

## 13. Persisting History

By default, history is in-memory only (lost on restart). To persist:
```
ui.history_file = "history.ndjson"
```
Each scan appends one JSON line. Loaded on startup. Trend chart uses
the last ui.history_max entries (default 50).

---

## 14. AST Analysis (Advanced)

The default heuristic analysis is fast and works for all 45 languages.
For more accurate complexity and function counts in C, Python, JavaScript,
Go, Java, and Rust, build with tree-sitter:

In CLion CMake options: -DMETIS_ENABLE_AST=ON

AST metrics overwrite heuristic results for the 6 covered languages.
All other languages continue using heuristics.

---

## 15. Activity Log

The Activity Log at the bottom of the dashboard shows all server events.
Use the Refresh, Clear, and Download buttons to manage it.

Also accessible at: GET /api/logs?limit=200

---

## 16. Shutdown

Click the Shutdown button in the toolbar, or:
```
POST /api/shutdown
```
The server confirms shutdown in the activity log before exiting.
