# Metis Code Analyzer Plus - Architecture

Version 2.5.1

## Overview

Metis Code Analyzer Plus is a single self-contained server binary plus a static
browser dashboard. The server owns the analysis engine and exposes results over
HTTP; the dashboard is a thin client that renders those results.

```
            +------------------+
  browser ->|  dashboard (web) |-- fetch --> REST API
            +------------------+                |
                                                v
                                      +---------------------+
                                      |     HTTP server     |  http.hpp
                                      +---------------------+
                                                |
                                                v
                                      +---------------------+
                                      |      Analyzer       |  analyzer.hpp
                                      +---------------------+
                                       |    |     |      |
                                  scanner metrics rules health
```

## Components

All server logic is header-only under `include/metis/codeanalyzer/`.

- **version.hpp** -- product name and version constants (MAJOR, MINOR, PATCH,
  STRING). The version is maintained manually and mirrored by CMake.
- **pson.hpp** -- configuration reader for the dotted-key PSON format.
- **json.hpp** -- minimal JSON value type and writer. Includes an explicit
  `const char*` overload on `Object::set()` so string literals are not
  silently converted to bool.
- **http.hpp** -- cross-platform HTTP/1.1 server: Winsock2 on Windows, BSD
  sockets on POSIX (Linux / macOS). Handles request parsing, route dispatch,
  TLS termination (OpenSSL, opt-in), and confined static file serving. The
  TLS layer negotiates X25519MLKEM768 (post-quantum hybrid, NIST FIPS 203
  ML-KEM) when OpenSSL 3.5+ is present; older OpenSSL falls back to classical
  groups automatically.
- **scanner.hpp** -- recursive directory walk, language detection by file
  extension (46 languages), exclusion list handling, and bounded file reads.
- **metrics.hpp** -- per-file line breakdown (total / code / comment / blank /
  over-length), cyclomatic complexity by decision-point count, maximum brace-
  nesting depth, and heuristic function count. When built with
  METIS_ENABLE_AST=ON, tree-sitter AST-accurate values replace heuristics for
  C, Python, JavaScript, Go, Java, and Rust.
- **rules.hpp** -- regex rule engine. Each rule carries a rule id, CWE
  reference, severity, health factor, and estimated remediation minutes. The
  catalog is loaded from `code_analyzer.pson` at startup; rules can be
  individually enabled or disabled in PSON.
- **health.hpp** -- five-factor health model (Robustness, Security, Efficiency,
  Transferability, Changeability) scored 0-100 and graded A-F. Technical-debt
  model: remediation minutes summed across findings, converted to hours and
  cost at the configured hourly rate. TQI is the mean of the five factors.
- **analyzer.hpp** -- orchestration: drives scanning, aggregates metrics and
  issues, computes duplicate blocks, extracts dependencies, evaluates health
  factors, and produces the AnalysisResult / ProjectReport.
- **auth.hpp** -- self-contained SHA-256 (no OpenSSL dependency) and an
  in-memory session store with sliding expiry. Backs admin login, sign-off,
  X-API-Key header authentication, and endpoint gating. Default credentials:
  `admin` / `Admin#2026` (must be changed before deployment).
- **log.hpp** -- thread-safe in-memory ring-buffer logger with level filtering
  and optional console echo. Backs the activity log and its REST API.
- **pdf.hpp** -- pure C++ PDF 1.4 writer (no external library). Produces
  quality reports with grade block, health-factor bars, summary, and issues.
- **license.hpp** -- license header scanner. Detects SPDX identifiers and
  common license preambles per file; backs GET /api/licenses.
- **ai.hpp** -- optional AI-assisted summary. Issues an HTTPS POST to an
  OpenAI-compatible provider (requires TLS build); backs POST /api/ai/summarize.
- **technologies.hpp** -- framework and library detection. Matches 70+
  import/include patterns across JavaScript, Python, Java, Go, Rust, and C++;
  backs GET /api/technologies.
- **ast.hpp** -- tree-sitter integration shim. Active only when built with
  METIS_ENABLE_AST=ON. Compiles the vendored tree-sitter runtime and six
  language grammars as static libraries; replaces heuristic metrics with AST-
  accurate values for the six supported languages.

`src/main.cpp` wires configuration loading, the analysis engine, all REST
routes, Prometheus metrics output, and the static dashboard.

`web/` contains the plain HTML/CSS/vanilla-JS dashboard: `index.html`,
`app.js`, `style.css`, `i18n.js` (eight-locale string table in 7-bit ASCII
with `\uXXXX` escapes), `logo.svg`, and `chart.umd.js` (Chart.js, vendored).

## Health-Factor Model

Each factor starts at 100 and is reduced by weighted issue density per
thousand lines of code. Severity weights: critical 10, major 5, minor 2,
info 1. Structural penalties:

- High average cyclomatic complexity reduces Robustness and Changeability.
- Low comment ratio reduces Transferability and Changeability.
- Duplicate block density reduces Changeability and Transferability.

Scores are clamped to 0-100 and graded: A (>=90), B (>=80), C (>=65),
D (>=50), F (<50). TQI is the unweighted mean of the five factors. Grade
thresholds are configurable in PSON via `health.grade.*` keys.

## Technical-Debt Model

Remediation minutes are summed across all findings and converted to hours and
a cost at `debt.hourly_rate`. The debt ratio compares remediation minutes to
an estimated development cost of `code_lines * debt.dev_minutes_per_line`.

## Security

- Static file serving canonicalizes the requested path and confirms it
  resolves within the configured web root before reading. Paths that escape
  the root return 404.
- Authentication: SHA-256 password hash stored in PSON (no plaintext).
  Session cookie is HttpOnly, SameSite=Lax. Login rate-limited (HTTP 429).
  X-API-Key header supported for headless / CI callers.
- Security headers on every response: X-Frame-Options, X-Content-Type-Options,
  Referrer-Policy, Content-Security-Policy, HSTS (when TLS active).
- TLS: X25519MLKEM768 post-quantum hybrid advertised first; AES-256-GCM
  cipher enforced via SSL_OP_CIPHER_SERVER_PREFERENCE.
- SIGPIPE ignored on POSIX so client disconnects do not terminate the server.

## Build Variants

| CMake flag             | Default | Effect                                        |
|------------------------|---------|-----------------------------------------------|
| `METIS_ENABLE_TLS`     | ON      | Link OpenSSL; serve HTTPS; post-quantum TLS   |
| `METIS_ENABLE_AST`     | OFF     | Compile tree-sitter + 6 grammars; AST metrics |

On Windows, `openssl-prebuilt/windows/` provides static OpenSSL 4.0.0 libs
automatically; no external install is required.

## Infrastructure Roadmap

Three subsystems are reserved for future C++20 implementation on Windows,
Linux, and macOS. No Docker, no external runtimes.

| Subsystem   | Endpoint          | PSON key             | Status  |
|-------------|-------------------|----------------------|---------|
| GPU         | GET /api/gpu      | gpu.enabled          | Planned |
| Kubernetes  | GET /api/kubernetes| kubernetes.enabled  | Planned |
| Containers  | GET /api/containers| containers.enabled  | Planned |

## Analysis Scope and Depth

Current analysis is line-, token-, and regex-heuristic based. Language-aware
comment handling covers all 46 supported languages. Rule matching is line-
scoped; interprocedural data-flow is not performed.

With METIS_ENABLE_AST=ON, tree-sitter provides accurate syntax trees for C,
Python, JavaScript, Go, Java, and Rust, replacing heuristic complexity and
function counts for those languages.

Planned increments (no Docker, no GTest, no PyTest):
- Deeper AST analysis via tree-sitter for all 46 languages.
- Persistent issue status storage via SQLite.
- WebSocket push for live scan progress.
- SAML / OIDC authentication option.
