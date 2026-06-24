# Background - Metis Code Analyzer Plus

## Table of Contents

1. [What This Is](#1-what-this-is)
2. [History and Purpose of Libraries for Code Analyzer Environments](#2-history-and-purpose-of-libraries-for-code-analyzer-environments)
   - 2.1 [Early Static Analysis (1970s - 1990s)](#21-early-static-analysis-1970s---1990s)
   - 2.2 [Rule-Based and Pattern Analysis (1990s - 2000s)](#22-rule-based-and-pattern-analysis-1990s---2000s)
   - 2.3 [AST-Driven and Quality-Factor Models (2000s - 2010s)](#23-ast-driven-and-quality-factor-models-2000s---2010s)
   - 2.4 [Cloud, AI, and Post-Quantum Era (2010s - Present)](#24-cloud-ai-and-post-quantum-era-2010s---present)
3. [How This Project Fits the Broader Ecosystem](#3-how-this-project-fits-the-broader-ecosystem)
   - 3.1 [Application-Intelligence / TQI-Style Platforms](#31-application-intelligence--tqi-style-platforms)
   - 3.2 [Deep Static Analysis (C/C++ Focus)](#32-deep-static-analysis-cc-focus)
   - 3.3 [Application Security (SAST)](#33-application-security-sast)
   - 3.4 [Hosted / Cloud Quality Platforms](#34-hosted--cloud-quality-platforms)
   - 3.5 [AST Parser Libraries](#35-ast-parser-libraries)
   - 3.6 [Where Metis Code Analyzer Plus Sits](#36-where-metis-code-analyzer-plus-sits)
4. [Design Philosophy](#4-design-philosophy)
   - 4.1 [Single Binary, Zero Runtime Dependencies](#41-single-binary-zero-runtime-dependencies)
   - 4.2 [REST-Only Interface](#42-rest-only-interface)
   - 4.3 [PSON Configuration](#43-pson-configuration)
   - 4.4 [Opt-In Capabilities](#44-opt-in-capabilities)
   - 4.5 [Honesty About Analysis Depth](#45-honesty-about-analysis-depth)
5. [The Scoring Model](#5-the-scoring-model)
6. [Infrastructure Roadmap (GPU, Kubernetes, Containers)](#6-infrastructure-roadmap-gpu-kubernetes-containers)
7. [Relationship to Named Products](#7-relationship-to-named-products)

---

## 1. What This Is

Metis Code Analyzer Plus is an application-intelligence and static-analysis
server for source trees. It scans a codebase and reports quality metrics,
detected issues, technical debt, and a set of health-factor grades, all served
through a browser dashboard and a REST API. It is part of the Metis Suite and
follows the suite conventions: pure C++20, zero external server dependencies,
PSON configuration, 7-bit ASCII source, and a single self-contained binary.
Targets Windows, Linux, and macOS.

Version: 2.5.1. Port: 8080 (configurable via the top-level `port` key in
`code_analyzer.pson`).

---

## 2. History and Purpose of Libraries for Code Analyzer Environments

### 2.1 Early Static Analysis (1970s - 1990s)

The intellectual roots of static code analysis reach back to lint (1978, Stephen
Johnson at Bell Labs), which scanned C source for type mismatches, unused
variables, and portability traps. lint introduced the idea that a separate tool -
distinct from the compiler - could enforce correctness and style at scale.

In the 1980s and 1990s mainframe environments brought their own formal discipline:
IBM's z/OS development pipeline required structured code review checkpoints, and
tools such as CA-Endevor and Panvalet enforced change management on COBOL and PL/I
source libraries. These were not static analyzers in the modern sense, but they
established the premise that source code is a managed artifact subject to policy
gates.

The purpose of analysis libraries in this era was narrow: catch obvious errors the
compiler did not, and enforce a small set of house rules. Analysis was lexical or
token-based - there were no ASTs, no control-flow graphs, no interprocedural
data-flow.

### 2.2 Rule-Based and Pattern Analysis (1990s - 2000s)

The 1990s brought a proliferation of rule-based checkers. PMD (2002, Java),
FindBugs / SpotBugs (2003, Java bytecode), and FxCop (.NET, 2002) formalized the
idea of a rule catalog: each rule has a name, a severity, a CWE or other reference,
and an estimated remediation cost. This is the model Metis Code Analyzer Plus
follows today.

Key library concepts that solidified in this period:

- Rule catalogs with per-rule enable/disable toggles.
- Severity tiers (critical, major, minor, informational).
- CWE (Common Weakness Enumeration) identifiers for security findings, linking
  tool output to government-sponsored vulnerability research.
- Technical-debt estimation: converting issue counts and severities into
  remediation hours and cost.
- Integration with build systems and CI pipelines.

The purpose expanded from "catch mistakes" to "measure and manage quality over
time."

### 2.3 AST-Driven and Quality-Factor Models (2000s - 2010s)

Two developments transformed the field in the 2000s:

First, CAST Software introduced the Total Quality Index (TQI) and the five-factor
health model (Robustness, Security, Efficiency, Transferability, Changeability).
Rather than reporting a flat list of issues, CAST Highlight and CAST Imaging
translate findings into scored health factors and a single composite grade. This
gave non-technical stakeholders a legible summary without losing the detail that
engineers need.

Second, the Clang project (2007, Apple / LLVM) demonstrated that a production-
quality compiler could expose its full AST as a library (libclang, 2010). This
made true path-sensitive, interprocedural analysis practical for open-source
tooling. Coverity, Klocwork, and PVS-Studio had been doing this commercially for
years; Clang made it available without a proprietary runtime.

tree-sitter (2018, GitHub) took a different approach: an incremental, error-
tolerant parser generator with a lightweight C runtime and grammar files for 50+
languages. It is not a full semantic analyzer but provides accurate syntax trees
cheaply, making it the leading choice for editor integrations (Neovim, Helix, Zed)
and, increasingly, for lightweight metric extraction.

The purpose of code-analyzer libraries broadened again: from "catch mistakes" and
"measure quality" to "provide actionable, scored intelligence that connects
engineering decisions to business outcomes."

### 2.4 Cloud, AI, and Post-Quantum Era (2010s - Present)

The 2010s brought SaaS deployment models (SonarCloud, CodeClimate, Codacy) that
removed the operations burden of self-hosting an analysis server. The 2020s added
two further dimensions:

AI-assisted summarization: large language models can interpret a static analysis
report and prioritize findings in plain language. OpenAI-compatible APIs have made
this available to any tool that can make an HTTPS call. Metis Code Analyzer Plus
integrates this as an optional feature (`ai.*` PSON keys, `POST /api/ai/summarize`)
so teams that want AI narrative can enable it without changing the core zero-
dependency architecture.

Post-quantum TLS: NIST finalized FIPS 203 (ML-KEM) in 2024. OpenSSL 3.5 ships
with the X25519MLKEM768 hybrid key exchange group, which combines classical X25519
with the ML-KEM-768 lattice-based KEM. Metis Code Analyzer Plus negotiates this
group by default when built with TLS enabled, providing forward secrecy against
future quantum adversaries for all dashboard and API traffic.

The purpose of code-analyzer libraries today extends to: govern quality at scale,
integrate with CI/CD pipelines, explain findings in natural language, and protect
all communications with current-generation cryptography.

---

## 3. How This Project Fits the Broader Ecosystem

### 3.1 Application-Intelligence / TQI-Style Platforms

These tools model quality as a set of weighted health factors and translate
findings into remediation cost. Metis draws on this methodology.

- **CAST Software** (CAST Highlight, CAST Imaging): the originator of the
  five-factor health model and the TQI. Full AST analysis, cloud dashboard,
  40+ languages. Enterprise-priced, requires server install. Metis draws on
  the methodology; shares no code and is not affiliated.

- **SonarQube / SonarCloud** (SonarSource): the most widely deployed
  open-source quality gate. Rule-based, 30+ languages, debt model, free
  Community edition. Self-hosted (SonarQube) or cloud (SonarCloud). Requires
  a JVM and a database.

- **TICS (TIOBE Quality Framework)**: health-factor model similar to CAST;
  used in safety-critical and regulated industries. Commercial, on-prem.

- **Squore** (Vector): commercial quality dashboard with configurable scoring;
  strong in automotive and defense.

- **Kiuwan** (Idera): cloud and on-prem static analysis with debt estimation
  and portfolio dashboards.

### 3.2 Deep Static Analysis (C/C++ Focus)

These tools perform interprocedural, path-sensitive, or formal analysis well
beyond what Metis provides today. The `METIS_ENABLE_AST` build option points
toward this direction.

- **Coverity** (Synopsys): interprocedural defect detection for C/C++/Java.
  The reference standard for safety-critical C/C++. Commercial.

- **Klocwork** (Perforce): deep C/C++/Java analysis; strong in embedded and
  automotive. Commercial.

- **PVS-Studio**: C/C++/C#/Java analyzer; well-regarded detailed diagnostics.
  Commercial with a free tier.

- **clang-tidy / clang Static Analyzer**: LLVM-based AST-driven analysis;
  free, integrates with the clang toolchain. No dashboard; complements Metis.

- **cppcheck**: free open-source C/C++ checker. No dashboard, command-line
  only. Complementary to Metis for C++ projects.

### 3.3 Application Security (SAST)

Metis includes CWE-mapped security rules but is not a replacement for a
dedicated SAST platform.

- **Checkmarx SAST**: enterprise SAST; broad language coverage; cloud and
  on-prem. Commercial.

- **Veracode**: cloud SAST; strong in compliance (PCI, HIPAA) reporting.
  Commercial.

- **Fortify Static Code Analyzer** (OpenText): enterprise SAST with a broad
  rule set. Commercial.

- **Semgrep**: open-source, pattern-based, fast; free tier available. Good
  for custom rule authoring.

### 3.4 Hosted / Cloud Quality Platforms

- **CodeClimate**: hosted maintainability and test coverage; supports multiple
  languages; free for open source.

- **Codacy / DeepSource**: automated code review on pull requests; cloud-based.

- **Embold** (Gridshore): code intelligence with architecture analysis.

### 3.5 AST Parser Libraries

These are parser infrastructure relevant to deeper per-language analysis.

- **tree-sitter**: incremental parser-generator framework with a C runtime and
  pre-built grammars for 50+ languages. Used by Neovim, Helix, Zed, and GitHub.
  MIT-licensed; the core and each grammar are vendorable C source with no runtime
  dependencies. This is the leading candidate for the Metis AST tier. Introduced
  as `METIS_ENABLE_AST=ON` (compile-time opt-in).
  Source: https://tree-sitter.github.io

- **libclang**: LLVM's stable C API for Clang's AST. Covers C, C++, and
  Objective-C with full language semantics. Requires LLVM/Clang libraries at
  build time. Most practical for C/C++ only.

### 3.6 Where Metis Code Analyzer Plus Sits

Metis is closest to SonarQube (self-hosted, quality-focused, REST API) but is
deliberately simpler: a single C++20 binary with zero runtime dependencies, a
PSON config file, and a plain HTML/CSS/JS dashboard. It runs on Windows, Linux,
and macOS without a JVM, database, or container. The health-factor model and TQI
are drawn from the CAST methodology. It is appropriate for teams that want a
lightweight, self-contained quality server rather than a full enterprise platform.

What Metis does not do today: full per-language AST analysis, interprocedural
data-flow, or enterprise portfolio dashboards. The codebase is designed to grow
toward some of those incrementally in plain C++20.

---

## 4. Design Philosophy

### 4.1 Single Binary, Zero Runtime Dependencies

The server ships as a single executable. No JVM, no Python runtime, no database,
no container. On Windows, build with MinGW-w64 GCC 13; on Linux/macOS with GCC or
Clang. OpenSSL is the only optional dependency, bundled as prebuilt static
libraries for Windows and found via CMake `find_package` on Linux/macOS. The
`web/` directory and `code_analyzer.pson` are staged next to the binary by CMake
at build time.

### 4.2 REST-Only Interface

The dashboard is a thin HTML/CSS/vanilla-JS client that communicates exclusively
through the same JSON REST API available to any external tool, CI pipeline, or
script. There is no separate "internal" communication path. Every feature visible
in the browser is reachable via a documented endpoint. See `docs/API.md` for the
full list and `docs/openapi.yaml` for the machine-readable specification.

### 4.3 PSON Configuration

All operational values live in `code_analyzer.pson`: listen port, TLS settings,
AI provider, rule catalog, scoring thresholds, project roots, exclude patterns,
webhook URL, and more. Nothing operational is hardcoded in the binary. The port
is the top-level `port` key and defaults to 8080.

### 4.4 Opt-In Capabilities

Capabilities that require additional dependencies or external services are opt-in:

- `METIS_ENABLE_TLS=ON` (default): links OpenSSL; enables HTTPS and post-quantum
  TLS. A build without TLS support serves plain HTTP on the same port.
- `METIS_ENABLE_AST=ON` (default OFF): compiles vendored tree-sitter and six
  grammars; enables AST-accurate complexity and function metrics for C, Python,
  JavaScript, Go, Java, and Rust.
- `ai.enabled = true` in PSON: activates AI-assisted summarization via an
  OpenAI-compatible HTTPS endpoint.

GPU, Kubernetes, and Container subsystems are reserved for future C++20 work.
Their API endpoints exist today and report current status from PSON; the
implementation arrives in a future release.

### 4.5 Honesty About Analysis Depth

v1.x and v2.x analysis is line-, token-, and regex-heuristic based. Complexity
and function counts are approximations, not AST-derived, unless `METIS_ENABLE_AST`
is on (for the six supported languages). The BACKGROUND.md, TODO.md, and
ARCHITECTURE.md documents state this plainly rather than implying deeper analysis
than exists.

---

## 5. The Scoring Model

Five health factors carry the assessment:

- **Robustness**: resistance to failure; penalized by high cyclomatic complexity
  and fragile error handling.
- **Security**: exposure to known weakness classes, mapped to CWE identifiers.
- **Efficiency**: patterns that waste work at scale.
- **Transferability**: how readily a new engineer picks up the code; driven by
  documentation density and structural clarity.
- **Changeability**: how safely the code can be modified; driven by complexity
  and duplication.

Each factor is scored 0-100 and graded A-F. Their mean is the Total Quality Index
(TQI). Detected issues each carry a severity and an estimated remediation time,
which aggregate into a technical-debt figure in hours and cost.

The grade thresholds and per-factor weights are configurable in PSON.

---

## 6. Infrastructure Roadmap (GPU, Kubernetes, Containers)

Three subsystems are reserved for future C++20 implementation on Windows, Linux,
and macOS. Their API endpoints exist today and report status from PSON:

| Endpoint          | ID         | PSON Key             | Status   |
|-------------------|------------|----------------------|----------|
| GET /api/gpu       | gpu        | gpu.enabled          | Planned  |
| GET /api/kubernetes| kubernetes | kubernetes.enabled   | Planned  |
| GET /api/containers| containers | containers.enabled   | Planned  |

The dashboard Infrastructure panel displays each subsystem's current status and
note pulled from the server. When implementation arrives, the `enabled` flag in
PSON will activate the feature; no dashboard change is required.

These subsystems will NOT use Docker, Doxygen, PyTest, GTest, or Jupyter. They
will be implemented in pure C++20, following the same zero-dependency server
architecture, and will target Windows (MinGW-w64 / MSVC), Linux (GCC/Clang), and
macOS (Clang).

---

## 7. Relationship to Named Products

Metis Code Analyzer Plus is an independent implementation. It shares no code with
CAST Software, SonarSource, or any other named product and is not affiliated with
or endorsed by any of them. References to those products describe the methodology
the design draws from or the landscape it operates in.

The health-factor model and TQI are drawn from the CAST methodology as a widely-
recognized quality framework. CAST Highlight and CAST Imaging are registered
products of CAST Software. SonarQube and SonarCloud are products of SonarSource
SA. All other product names are trademarks of their respective owners.
