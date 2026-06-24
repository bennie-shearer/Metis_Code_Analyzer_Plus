# Metis Code Analyzer Plus

Version 2.4.2

Application intelligence and static code analysis for source trees. Metis Code
Analyzer Plus scans a codebase and reports quality metrics, technical debt,
detected issues, dependency information, and five health factors (Robustness,
Security, Efficiency, Transferability, Changeability), all served through a
browser dashboard and a REST API.

Pure C++20 server with zero external runtime dependencies. Plain HTML/CSS/
vanilla-JS dashboard with no frameworks and no build step. Charts rendered by
Chart.js, vendored locally under `web/` (no CDN). All operational values live
in `code_analyzer.pson`. Targets Windows, Linux, and macOS.

Default port: **8080** (configurable via the top-level `port` key in
`code_analyzer.pson`).

---

## What It Measures

- **Line breakdown** per file: total, code, comment, blank, and over-length lines.
- **Cyclomatic complexity** by the decision-point method, plus maximum brace
  nesting depth and a heuristic function count.
- **Issue detection** via a configurable rule engine. Each finding carries a
  rule id, a CWE reference where applicable, a severity, the affected health
  factor, and an estimated remediation effort in minutes.
- **Duplication**: repeated normalized line blocks across the tree.
- **Dependencies**: include / import / require targets per file.
- **Technical debt**: aggregated remediation minutes, hours, cost, and a debt
  ratio against an estimated development cost.
- **Health factors**: Robustness, Security, Efficiency, Transferability, and
  Changeability, each scored 0-100 and graded A-F, plus a Total Quality Index.

---

## Build

Requires a C++20 compiler and CMake 3.20 or newer.

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The build stages `web/` and `code_analyzer.pson` next to the binary.

### Optional capabilities

| CMake flag               | Default | What it adds                                          |
|--------------------------|---------|-------------------------------------------------------|
| `METIS_ENABLE_TLS=ON`    | ON      | Post-quantum TLS via OpenSSL (X25519MLKEM768/ML-KEM)  |
| `METIS_ENABLE_AST=ON`    | OFF     | AST-accurate metrics via vendored tree-sitter         |

On Windows, `openssl-prebuilt/windows/` provides static OpenSSL 4.0.0 libs
automatically; no external install required.

---

## Platforms

| Platform              | Compiler          | Status              |
|-----------------------|-------------------|---------------------|
| Windows               | MinGW-w64 GCC 13  | Verified            |
| Linux                 | GCC 13 / Clang    | Verified            |
| macOS                 | Clang             | Verified by inspection |

All OS-specific code is isolated behind `_WIN32`; the POSIX branch uses only
standard headers and contains no Linux-only calls.

---

## Run

```
cd build
./metis_code_analyzer_plus code_analyzer.pson
```

Open `http://localhost:8080` (or `https://localhost:8080` with TLS enabled).

---

## Configuration

All operational settings live in `code_analyzer.pson`. Nothing operational is
hardcoded. Key settings:

| Key               | Default | Description                                  |
|-------------------|---------|----------------------------------------------|
| `port`            | 8080    | Listen port (HTTP or HTTPS)                  |
| `tls.enabled`     | true    | Enable post-quantum TLS                      |
| `ai.enabled`      | false   | Enable AI-assisted summaries                 |
| `scan.root`       | "."     | Default directory to scan                    |
| `auth.enabled`    | true    | Require admin login                          |

See `docs/CONFIGURATION.md` for the full reference.

---

## Operations

- **Admin login**: when `auth.enabled` is true, the dashboard requires sign-in.
  Default credentials are `admin` / `Admin#2026`; change before deployment.
- **Post-quantum TLS**: build with `METIS_ENABLE_TLS=ON` (default). Negotiates
  `X25519MLKEM768` (NIST FIPS 203 ML-KEM) when linked against OpenSSL 3.5+;
  older OpenSSL falls back to classical groups automatically. Generate a
  locally-trusted certificate with `certs\gen_certs.ps1` (Windows).
- **AI Insights**: optional. Set `ai.enabled = true` and configure `ai.endpoint`,
  `ai.model`, and `ai.api_key` in PSON. Requires a TLS-enabled build. Trigger
  via the AI Insights panel or `POST /api/ai/summarize`.
- **GPU / Kubernetes / Containers**: endpoints `/api/gpu`, `/api/kubernetes`,
  `/api/containers` exist and report "planned". Implementation is reserved for
  future C++20 work on Windows, Linux, and macOS. No Docker, no containers at
  runtime.
- **Exports**: PDF, JSON, and CSV reports available from the dashboard toolbar
  or via `GET /api/report.pdf`, `/api/report.json`, `/api/report.csv`.
- **Prometheus metrics**: `GET /metrics`.
- **Activity log**: `GET /api/logs`, `POST /api/logs/clear`.
- **CI/CD quality gate**: `GET /api/gate`; `gate.*` PSON keys; optional
  non-zero exit on failure for pipeline integration.
- **Internationalization**: eight locales (US/UK English, French, German,
  Spanish, Italian, Brazilian Portuguese, Japanese).

---

## REST API

The dashboard is a thin client; every interaction goes through the JSON API.
See `docs/API.md` for the full list. `docs/openapi.yaml` is the machine-readable
OpenAPI 3.0 specification.

Highlights:

| Method | Endpoint               | Description                        |
|--------|------------------------|------------------------------------|
| GET    | /api/version           | Product version and rule count     |
| GET    | /api/results           | Latest analysis report             |
| GET    | /api/files             | Per-file metrics                   |
| GET    | /api/issues            | Issue list (filterable)            |
| POST   | /api/scan              | Trigger a scan                     |
| GET    | /api/report.pdf        | PDF quality report                 |
| GET    | /api/report.json       | Full JSON export                   |
| GET    | /api/report.csv        | Issues CSV export                  |
| GET    | /api/security          | TLS status and cipher info         |
| GET    | /api/config            | Server configuration               |
| GET    | /api/gpu               | GPU subsystem status               |
| GET    | /api/kubernetes        | Kubernetes subsystem status        |
| GET    | /api/containers        | Container subsystem status         |
| POST   | /api/ai/summarize      | AI-assisted findings summary       |
| GET    | /metrics               | Prometheus metrics                 |
| POST   | /api/login             | Authenticate                       |
| POST   | /api/logout            | End session                        |
| POST   | /api/shutdown          | Graceful shutdown                  |

---

## Supported Languages

46 languages recognized by file extension. Highlights:

C++, C, C/C++ Header, Python, JavaScript, TypeScript, Java, C#, Go, Rust,
Ruby, PHP, COBOL, Shell, SQL, Kotlin, Swift, Scala, Dart, R, Lua, Perl,
Objective-C, Fortran, HLASM, REXX, JCL, PL/I, Haskell, Elixir, Ada,
PowerShell, MATLAB, Groovy, Zig, ABAP, F#, Erlang, Clojure, Crystal, Nim,
D, OCaml, VHDL, Verilog, Visual Basic 6.

Live list: `GET /api/languages`.

---

## Documentation

| File                          | Contents                                  |
|-------------------------------|-------------------------------------------|
| `README.md`                   | This file                                 |
| `docs/API.md`                 | Full REST API reference                   |
| `docs/ARCHITECTURE.md`        | Internal design and extension points      |
| `docs/BACKGROUND.md`          | History, ecosystem context, philosophy    |
| `docs/CHANGELOG.md`           | Per-version change log                    |
| `docs/CI.md`                  | CI/CD integration (GitHub Actions, etc.)  |
| `docs/CONFIGURATION.md`       | Full PSON configuration reference         |
| `docs/GOVERNMENT_REFERENCES.md`| NIST, MITRE, IETF standards used         |
| `docs/HOWTO.md`               | Step-by-step operational guides           |
| `docs/SUPPORT.md`             | Support and security disclosure           |
| `docs/TODO.md`                | Open and completed work                   |
| `docs/openapi.yaml`           | OpenAPI 3.0 machine-readable API spec     |
| `docs/THIRD_PARTY.md`         | Third-party component notices             |
| `LICENSE`                     | MIT License                               |

---

## License

MIT License. See `LICENSE`.

Copyright (c) 2026 Bennie Shearer (Retired)

Written in honor of the Chones Family:
- Nicholas James Chones, December 3, 1926 - September 29, 2009, USN
- Anastasia Louis Chones, January 13, 1927 - January 27, 2018
