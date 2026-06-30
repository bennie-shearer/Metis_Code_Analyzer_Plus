# Metis Code Analyzer Plus

Version 2.7.14

Application intelligence and static code analysis for source trees. Metis Code
Analyzer Plus scans a codebase and reports quality metrics, technical debt,
detected issues, dependency information, and six health factors (Robustness,
Security, Efficiency, Transferability, Changeability, Portability), served
through a browser dashboard and a REST API.

Pure C++20 server. Zero external runtime dependencies. Plain HTML/CSS/vanilla-JS
dashboard -- no frameworks, no build step. Charts via bundled Chart.js. All
operational parameters live in `code_analyzer.pson`. Targets Windows, Linux,
and macOS.

Default port: **8443** (configurable via the top-level `port` key in
`code_analyzer.pson`).

---

## What It Measures

- **Line breakdown** per file: total, code, comment, blank, and over-length lines.
- **Cyclomatic complexity** by the decision-point method, plus maximum brace
  nesting depth and heuristic function count (AST-accurate with METIS_ENABLE_AST).
- **Issue detection** via a configurable regex rule engine. Each finding carries
  a rule ID, a CWE reference where applicable, a severity, the affected health
  factor, and an estimated remediation effort in minutes. 35 built-in rules;
  all configurable and extendable in PSON.
- **Duplication**: repeated normalized line blocks across the tree.
- **Dependencies**: include / import / require targets per file.
- **Technical debt**: aggregated remediation minutes, hours, cost, and a debt
  ratio against an estimated development cost.
- **Health factors**: six factors (Robustness, Security, Efficiency,
  Transferability, Changeability, Portability) each scored 0-100 and graded
  A-F, plus a Total Quality Index (TQI).
- **Technology inventory**: framework and library detection from import patterns.

---

## Build

Requires a C++20 compiler and CMake 3.20 or newer.

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The build stages `web/` and `code_analyzer.pson` next to the binary.

### Optional capabilities

| CMake flag               | Default | What it adds                                           |
|--------------------------|---------|--------------------------------------------------------|
| `METIS_ENABLE_TLS=ON`    | ON      | Post-quantum TLS via OpenSSL (X25519MLKEM768 / ML-KEM) |
| `METIS_ENABLE_AST=ON`    | OFF     | AST-accurate metrics via vendored tree-sitter          |

On Windows, `openssl-prebuilt/windows/` provides static OpenSSL 4.0.0 libs
automatically; no external install is required.

---

## Platforms

| Platform  | Compiler          | Status                   |
|-----------|-------------------|--------------------------|
| Windows   | MinGW-w64 GCC 13  | Primary / verified       |
| Linux     | GCC 13 / Clang    | Verified                 |
| macOS     | Clang             | Verified by inspection   |

All OS-specific code is isolated behind `#ifdef _WIN32`. The POSIX branch uses
only standard C++20 headers and contains no Linux-only calls.

---

## Run

```
cd build
./metis_code_analyzer_plus code_analyzer.pson
```

Open `http://localhost:8443` (or `https://localhost:8443` with TLS enabled).

---

## Configuration

All operational settings live in `code_analyzer.pson`. Nothing operational is
hardcoded. Every parameter is documented inline in that file.

Key settings:

| Key                   | Default       | Description                              |
|-----------------------|---------------|------------------------------------------|
| `port`                | `8443`        | Listen port (HTTP or HTTPS)              |
| `tls.enabled`         | `true`        | Enable post-quantum TLS                  |
| `tls.cert_file`       | `""`          | PEM cert path (empty = auto self-signed) |
| `ai.enabled`          | `false`       | Enable AI-assisted summaries             |
| `scan.root`           | `"."`         | Default directory to scan                |
| `auth.enabled`        | `true`        | Require admin login                      |
| `gate.enabled`        | `false`       | Enable CI/CD quality gate                |
| `technologies.enabled`| `true`        | Technology inventory detection           |
| `gpu.enabled`         | `false`       | GPU probe (impl planned)                 |
| `kubernetes.enabled`  | `false`       | Kubernetes probe (impl planned)          |
| `containers.enabled`  | `false`       | Container probe (impl planned)           |

See `docs/CONFIGURATION.md` for the full reference.

---

## Operations

- **Admin login**: when `auth.enabled = true`, the dashboard requires sign-in.
  Default credentials are `admin` / `Admin#2026`; change before deployment by
  hashing a new password via `POST /api/hash-password` and updating
  `auth.password_sha256` in PSON.
- **Post-quantum TLS**: build with `METIS_ENABLE_TLS=ON` (default). Negotiates
  `X25519MLKEM768` (NIST FIPS 203 ML-KEM) when linked against OpenSSL 3.5+;
  older OpenSSL falls back to classical groups. On Windows generate a locally-
  trusted certificate with `certs\gen_certs.ps1`.
- **AI Insights**: set `ai.enabled = true` and configure `ai.endpoint`,
  `ai.model`, and `ai.api_key` in PSON. Requires a TLS-enabled build. Trigger
  via the AI Insights panel or `POST /api/ai/summarize`. Compatible with any
  OpenAI-style API (Anthropic, OpenAI, local models).
- **GPU / Kubernetes / Containers**: detection probes exist at `/api/gpu`,
  `/api/kubernetes`, `/api/containers`. Full C++20 implementation planned for
  Windows, Linux, and macOS. No Docker dependency at build or runtime.
- **Exports**: PDF, JSON, and CSV reports from the dashboard toolbar or
  `GET /api/report.pdf`, `/api/report.json`, `/api/report.csv`.
- **Prometheus metrics**: `GET /metrics`.
- **Quality gate**: configure via `gate.*` PSON keys. `GET /api/gate` for
  headless CI. `gate.exit_nonzero = true` causes a non-zero exit on failure.
- **Issue lifecycle**: mark findings open / accepted / wontfix / false_positive
  via the Status column in the Issues table. Statuses persist across restarts
  via `ui.issue_status_file` (default: `data/issue_status.ndjson`).
- **Multi-project**: define `project.N.name` / `project.N.root` in PSON and
  switch the active project via `POST /api/project/select`.
- **Watch mode**: `scan.watch = true` polls the scan root and rescans
  automatically on file changes.
- **Internationalization**: eight locales (US/UK English, French, German,
  Spanish, Italian, Brazilian Portuguese, Japanese).

---

## REST API

Every dashboard interaction goes through the JSON API. The dashboard is a thin
client; the API is the contract. See `docs/API.md` for the full reference.
Machine-readable OpenAPI 3.0 spec: `docs/openapi.yaml`.

| Method | Endpoint                | Description                          |
|--------|-------------------------|--------------------------------------|
| GET    | /api/version            | Product version and rule count       |
| GET    | /api/results            | Latest analysis report               |
| GET    | /api/files              | Per-file metrics                     |
| GET    | /api/issues             | Issue list                           |
| WS     | /api/scan/ws            | Live scan progress (WebSocket)       |
| POST   | /api/scan               | Trigger a scan (HTTP fallback)       |
| GET    | /api/report.pdf         | PDF quality report                   |
| GET    | /api/report.json        | Full JSON export                     |
| GET    | /api/report.csv         | Issues CSV export                    |
| GET    | /api/history            | Scan trend history                   |
| GET    | /api/licenses           | License header inventory             |
| GET    | /api/technologies       | Framework/library inventory          |
| GET    | /api/gate               | Quality gate result                  |
| GET    | /api/security           | TLS status and cipher info           |
| GET    | /api/config             | Server configuration (dashboard use) |
| GET    | /api/projects           | Multi-project list                   |
| POST   | /api/project/select     | Switch active project                |
| GET    | /api/gpu                | GPU subsystem status                 |
| GET    | /api/kubernetes         | Kubernetes subsystem status          |
| GET    | /api/containers         | Container subsystem status           |
| POST   | /api/ai/summarize       | AI-assisted findings summary         |
| GET    | /api/issue/status       | Get issue lifecycle status           |
| POST   | /api/issue/status       | Set issue lifecycle status           |
| GET    | /api/source             | Source preview for a finding         |
| GET    | /api/browse             | Directory browser                    |
| GET    | /api/languages          | Supported language list              |
| GET    | /api/health             | Server liveness probe                |
| GET    | /api/session            | Current auth session state           |
| POST   | /api/login              | Authenticate                         |
| POST   | /api/logout             | End session                          |
| GET    | /api/logs               | Activity log entries                 |
| POST   | /api/logs/clear         | Clear activity log                   |
| GET    | /api/hash-password      | SHA-256 hash utility (setup only)    |
| POST   | /api/hash-password      | SHA-256 hash utility (setup only)    |
| GET    | /metrics                | Prometheus metrics                   |
| POST   | /api/shutdown           | Graceful shutdown                    |

---

## Supported Languages (46)

C++, C, C/C++ Header, Python, JavaScript, TypeScript, Java, C#, Go, Rust,
Ruby, PHP, COBOL, Shell, SQL, Kotlin, Swift, Scala, Dart, R, Lua, Perl,
Objective-C, Fortran, HLASM, REXX, JCL, PL/I, Haskell, Elixir, Ada,
PowerShell, MATLAB, Groovy, Zig, ABAP, F#, Erlang, Clojure, Crystal, Nim,
D, OCaml, VHDL, Verilog, Visual Basic 6.

Full list: `GET /api/languages`.

---

## Documentation

| File                             | Contents                                   |
|----------------------------------|--------------------------------------------|
| `README.md`                      | This file                                  |
| `docs/API.md`                    | Full REST API reference                    |
| `docs/ARCHITECTURE.md`           | Internal design and extension points       |
| `docs/BACKGROUND.md`             | History, ecosystem context, philosophy     |
| `docs/CHANGELOG.md`              | Full per-version change log                |
| `docs/CI.md`                     | CI/CD integration guide                    |
| `docs/CONFIGURATION.md`          | Full PSON configuration reference          |
| `docs/GOVERNMENT_REFERENCES.md`  | NIST, MITRE, IETF standards used           |
| `docs/HOWTO.md`                  | Step-by-step operational guides            |
| `docs/SUPPORT.md`                | Support and security disclosure            |
| `docs/TODO.md`                   | Open and completed work                    |
| `docs/openapi.yaml`              | OpenAPI 3.0 machine-readable API spec      |
| `docs/THIRD_PARTY.md`            | Third-party component notices              |
| `LICENSE`                        | MIT License                                |

---

## Acknowledgments

| Organization                | Website                                 |
|-----------------------------|-----------------------------------------|
| CLion by JetBrains s.r.o.  | https://www.jetbrains.com/clion/        |
| WebStorm by JetBrains s.r.o.| https://www.jetbrains.com/webstorm/    |
| Claude by Anthropic PBC     | https://www.anthropic.com/             |

*Special thanks to all mentors through the years.*

---

## License

MIT License. See `LICENSE`.

Copyright (c) 2026 Bennie Shearer (Retired)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

DISCLAIMER: This software provides code analysis capabilities. Users are
responsible for proper configuration, analysis testing, and compliance with
applicable regulations and licensing requirements.

---

## Dedication

Written in honor of the Chones Family:
- Nicholas James Chones, December 3, 1926 - September 29, 2009, USN
- Anastasia Louis Chones, January 13, 1927 - January 27, 2018

---

## Author

Bennie Shearer (Retired)
