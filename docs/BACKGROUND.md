# Background - Metis Code Analyzer Plus

## Table of Contents

1. [What This Is](#1-what-this-is)
2. [History and Purpose of Libraries for Code Analyzer Environments](#2-history-and-purpose-of-libraries-for-code-analyzer-environments)
   - 2.1 [Early Static Analysis (1970s-1990s)](#21-early-static-analysis-1970s-1990s)
   - 2.2 [Rule-Based and Pattern Analysis (1990s-2000s)](#22-rule-based-and-pattern-analysis-1990s-2000s)
   - 2.3 [AST-Driven and Quality-Factor Models (2000s-2010s)](#23-ast-driven-and-quality-factor-models-2000s-2010s)
   - 2.4 [Cloud, AI, and Post-Quantum Era (2010s-Present)](#24-cloud-ai-and-post-quantum-era-2010s-present)
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
   - 4.3 [PSON Configuration: Single Source of Truth](#43-pson-configuration-single-source-of-truth)
   - 4.4 [Opt-In Capabilities](#44-opt-in-capabilities)
   - 4.5 [Portability Across Windows, Linux, and macOS](#45-portability-across-windows-linux-and-macos)
   - 4.6 [Honesty About Analysis Depth](#46-honesty-about-analysis-depth)
5. [The Scoring Model](#5-the-scoring-model)
6. [Infrastructure Roadmap (GPU, Kubernetes, Containers)](#6-infrastructure-roadmap-gpu-kubernetes-containers)
7. [Security Architecture](#7-security-architecture)
8. [Relationship to Named Products](#8-relationship-to-named-products)

---

## 1. What This Is

Metis Code Analyzer Plus is an application-intelligence and static-analysis
server for source trees. It scans a codebase and reports quality metrics,
detected issues, technical debt, and a set of health-factor grades, all served
through a browser dashboard and a REST API.

It is part of the Metis Suite and follows suite conventions: pure C++20, zero
external server dependencies, PSON configuration, 7-bit ASCII source, and a
single self-contained binary. It targets Windows, Linux, and macOS equally.

Version: 2.7.4. Port: 8443 (configurable via the top-level `port` key in
`code_analyzer.pson`). Default credentials: `admin` / `Admin#2026` (change
before deployment). TLS enabled by default with post-quantum hybrid key
exchange (X25519MLKEM768, NIST FIPS 203 ML-KEM).

---

## 2. History and Purpose of Libraries for Code Analyzer Environments

### 2.1 Early Static Analysis (1970s-1990s)

Static code analysis has roots in the 1970s. The original UNIX `lint` tool
(1978, S.C. Johnson, Bell Labs) was the first widely-used static analyzer for
the C language. It performed type checking beyond the compiler's scope,
flagging unreachable code, uninitialized variables, and suspicious constructs.
`lint` established the foundational vocabulary of static analysis: rules,
findings, severity, and suppression.

Through the 1980s, formal methods researchers developed tools like SPARK Ada
(program correctness proofs) and early data-flow analyzers. These were largely
academic or defense-sector tools with steep adoption barriers. The dominant
industry concern was correctness in safety-critical software: avionics,
nuclear, and medical devices.

The 1990s saw three significant developments:
- PC-based development exploded, bringing large codebases into commercial
  settings where defect cost was measured in dollars, not lives.
- Object-oriented languages (C++, Java) created new categories of defects
  (null pointer dereference, resource leaks, type-unsafe casts) that
  procedural-era tools did not model.
- Rule-based analyzers and style checkers (PC-lint, Flexelint, PMD for Java)
  emerged as practical tools that engineering teams could integrate into their
  build processes without formal-methods expertise.

### 2.2 Rule-Based and Pattern Analysis (1990s-2000s)

Rule-based analysis dominated the late 1990s and 2000s. The approach is
conceptually simple: define a library of patterns (textual or AST-structural)
that represent known-bad constructs, match those patterns against source text
or parse trees, and emit a list of findings. Tools in this era include:

- **FindBugs** (2003, Bill Pugh and David Hovemeyer): the first widely-adopted
  open-source bytecode analyzer for Java. Its influence persists in the
  SpotBugs fork maintained today.
- **Checkstyle**: style and convention enforcement for Java, driven by a rule
  XML that teams could customize.
- **PC-lint / FlexeLint** (Gimpel Software): long-lived commercial C/C++
  analyzer with an extremely large rule set and heavy suppression annotation
  culture.
- **MISRA-C** (Motor Industry Software Reliability Association, 1998 and 2004):
  a rulebook for safety-critical embedded C. MISRA-C is a standard, not a
  tool; a generation of checkers (Polyspace, LDRA, PC-lint) was built to
  validate against it.

The regex-pattern approach used in Metis Code Analyzer Plus is a deliberate
choice to remain in this tradition for the rule catalog: simple, auditable,
zero-dependency, and deployable anywhere.

### 2.3 AST-Driven and Quality-Factor Models (2000s-2010s)

Two advances characterized the 2000s decade:

**Abstract Syntax Tree analysis.** As compiler front-ends became open (GCC,
then Clang), tool authors gained access to the full parse tree rather than
lexical text. This enabled data-flow analysis (reaching definitions, taint
tracking, alias analysis), interprocedural analysis, and path-sensitive
checkers. Tools such as Coverity Static Analyzer (founded 2002), Klocwork,
and the Clang Static Analyzer (2007) represent this generation. Their accuracy
on complex pointer and concurrency bugs is dramatically higher than regex-based
tools at the cost of much longer analysis times and compiler-specific
integration.

**Quality-factor models.** The Software Quality Institute at Carnegie Mellon
and researchers at CAST (now Casewise) developed multi-dimensional quality
frameworks that measure code health across orthogonal dimensions rather than
reporting a flat list of defects. The most influential formalization is the
ISO 25010:2011 Software Quality Model, which defines quality characteristics
including reliability, security, maintainability, and portability. CAST's
Automated Function Point and application-intelligence platform translates these
into a Total Quality Index (TQI) score. Metis Code Analyzer Plus uses a
six-factor health model (Robustness, Security, Efficiency, Transferability,
Changeability, Portability) directly inspired by this lineage.

The cyclomatic complexity metric (Thomas McCabe, 1976) and the Halstead
complexity measures (Maurice Halstead, 1977) remain standard inputs to quality
models today.

### 2.4 Cloud, AI, and Post-Quantum Era (2010s-Present)

Three trends define contemporary code analysis:

**Cloud-native SaaS delivery.** SonarQube (SonarSource, 2008), then SonarCloud,
moved quality dashboards to the browser and integrated with popular CI
platforms (GitHub, GitLab, Jenkins). The "quality gate" concept (block a merge
if quality thresholds are not met) became standard DevSecOps practice.

**AI-assisted analysis.** Large language models (GPT-4, Claude, Gemini) are now
used for both finding generation ("what looks wrong here?") and remediation
("how do I fix this?"). GitHub Copilot Autofix (2024) embeds LLM-powered
remediation directly into the security finding workflow. Metis Code Analyzer
Plus integrates AI-assisted summaries via a configurable provider endpoint
(`POST /api/ai/summarize`) without requiring a specific vendor.

**Post-quantum cryptography.** NIST finalized ML-KEM (CRYSTALS-Kyber, FIPS 203,
2024) and ML-DSA (CRYSTALS-Dilithium, FIPS 204). For tools that serve security
dashboards over TLS, post-quantum key exchange is a meaningful safeguard against
"harvest now, decrypt later" attacks. Metis Code Analyzer Plus negotiates
X25519MLKEM768 (a hybrid of X25519 and ML-KEM-768) as its preferred TLS group
when built with OpenSSL 3.5 or later.

---

## 3. How This Project Fits the Broader Ecosystem

### 3.1 Application-Intelligence / TQI-Style Platforms

**CAST Highlight / CAST Imaging** (CAST Software): The commercial originator of
the TQI and health-factor model. Full AST-level analysis, SaaS delivery,
enterprise pricing. Metis Code Analyzer Plus uses the same conceptual framework
(six health factors, weighted issue density, grade A-F) at zero cost and with
a self-hosted, zero-dependency deployment model.

**SonarQube / SonarCloud** (SonarSource): The dominant open-core quality
platform. Rich rule catalogs for 25+ languages, tight IDE and CI integration,
SaaS and on-premise editions. Requires a Java runtime and a database backend.
Metis targets the use case where teams want a single self-contained binary
without the JVM/PostgreSQL stack.

### 3.2 Deep Static Analysis (C/C++ Focus)

**Coverity Static Analyzer** (Synopsys): Industry-standard interprocedural
analysis with path-sensitive checkers. Used extensively in automotive, avionics,
and government software. Compilation-unit level integration; very long analysis
times for large trees. Metis does not compete on analysis depth but is
dramatically simpler to deploy and run.

**Clang Static Analyzer / clang-tidy**: Open-source, compiler-integrated,
extremely accurate for C/C++. Requires a Clang build toolchain. Metis is
language-agnostic and requires no language toolchain.

**PC-lint Plus** (Gimpel Software): Long-lived commercial C/C++ analyzer with
a vast rule set and strong MISRA compliance checking. Metis includes a
MISRA-C heuristic subset (rules 16, 17, 34) for teams that want signal without
the full tool integration overhead.

### 3.3 Application Security (SAST)

**Semgrep**: Fast, multilingual pattern-based scanner with a large public rule
registry. Open-source core with commercial cloud. Metis's regex rule engine is
conceptually similar; Semgrep uses a structured AST-pattern DSL (more powerful
but more complex to author rules).

**Checkmarx / Veracode**: Enterprise SAST platforms with deep taint tracking
and compliance reporting. SaaS delivery; significant per-scan latency. Metis
trades depth for zero-external-dependency self-hosted operation.

**Bandit** (Python Security): Pure-Python SAST for Python codebases. Single
language; Metis covers 46 languages with the same binary.

### 3.4 Hosted / Cloud Quality Platforms

**CodeClimate Quality**: SaaS platform wrapping multiple open-source analyzers
with a unified quality dashboard. Requires GitHub/GitLab hosting. Metis is
fully self-hosted with no cloud dependency.

**DeepSource**: AI-assisted issue detection with autofix suggestions. SaaS.
Metis's `POST /api/ai/summarize` provides a similar AI-assist loop via any
OpenAI-compatible endpoint without vendor lock-in.

### 3.5 AST Parser Libraries

**Tree-sitter** (Max Brunsfeld, GitHub, 2018): Incremental, error-tolerant
parser generator used by Neovim, GitHub, and many editors. Metis bundles
tree-sitter as an optional build flag (`METIS_ENABLE_AST=ON`) for accurate
per-function metrics on C, Python, JavaScript, Go, Java, and Rust. Grammars
for all 46 supported languages are planned as tree-sitter support matures.

**ANTLR** (Terence Parr): Grammar-first parser generator used by many
language-specific analyzers. Metis does not use ANTLR; tree-sitter's
incremental parser is better suited to the partial-parse, error-recovery
requirements of source analysis.

### 3.6 Where Metis Code Analyzer Plus Sits

Metis Code Analyzer Plus occupies a specific niche:

| Dimension        | Metis Code Analyzer Plus                                 |
|------------------|----------------------------------------------------------|
| Deployment       | Self-hosted; single binary; zero server dependencies     |
| Analysis depth   | Regex rules + optional tree-sitter AST                   |
| Language support | 46 languages from one binary                             |
| Quality model    | TQI + six health factors (CAST-lineage)                  |
| Security         | Post-quantum TLS (X25519MLKEM768, FIPS 203 ML-KEM)       |
| AI integration   | Optional; any OpenAI-compatible endpoint                 |
| CI/CD            | Quality gate with non-zero exit; OpenAPI spec; REST API  |
| License          | MIT                                                      |
| Build            | C++20; CMake/Ninja; Windows/Linux/macOS                  |
| Runtime deps     | None (OpenSSL bundled for Windows; tree-sitter vendored) |

It is not a replacement for Coverity, SonarQube, or Semgrep in environments
that need deep interprocedural analysis or a cloud-managed rule registry. It
is a complement or a primary tool for teams that need fast, fully self-hosted,
zero-dependency quality visibility across a polyglot codebase.

---

## 4. Design Philosophy

### 4.1 Single Binary, Zero Runtime Dependencies

The server is a single executable with no runtime dependencies beyond the
operating system. There is no JVM, no Python interpreter, no database, no
message queue, and no container runtime required at deployment time. The web
dashboard is served directly from a `web/` directory; no separate web server
is needed. OpenSSL is statically linked (Windows prebuilt, system library on
Linux/macOS). Tree-sitter is vendored in `vendor/` and compiled in if
`METIS_ENABLE_AST=ON` is set.

This design principle is inherited from the Metis Suite: every Metis product
is a single self-contained binary that a developer can drop on a machine and
run immediately.

### 4.2 REST-Only Interface

Every dashboard interaction goes through the JSON REST API. The HTML/CSS/JS
client is a thin consumer of that API with no server-side rendering. This
means:

- The API is the contract; the dashboard can be replaced or extended without
  touching the server.
- Headless CI use (curl, scripts) is first-class: authenticate with an API
  key, GET /api/gate, check the exit code.
- The OpenAPI specification (`docs/openapi.yaml`) is the authoritative machine-
  readable description of the API surface.

### 4.3 PSON Configuration: Single Source of Truth

Every operational parameter lives in `code_analyzer.pson`. No hardcoded ports,
paths, credentials, rule patterns, or thresholds exist in the binary. The PSON
file is the single source of truth for configuration, extensively documented
inline with comments explaining every key.

This approach enables configuration-as-code workflows: check the PSON file
into source control, diff changes over time, and reproduce any historical
configuration by checking out the corresponding commit.

### 4.4 Opt-In Capabilities

Optional capabilities (TLS, AST analysis, AI integration) are controlled by
CMake build flags and PSON keys respectively. The zero-flag build produces a
working analyzer that scans code and serves a dashboard with no external
dependency at all. Teams add capabilities as their needs and infrastructure
evolve:

- `METIS_ENABLE_TLS=ON` (default) adds post-quantum TLS.
- `METIS_ENABLE_AST=ON` adds tree-sitter AST metrics.
- `ai.enabled = true` in PSON enables AI-assisted summaries.
- `gate.enabled = true` enables CI/CD quality gates.

### 4.5 Portability Across Windows, Linux, and macOS

All OS-specific code is isolated behind `#ifdef _WIN32` / `#else` guards.
The POSIX branch uses only the C++20 standard library and POSIX-standard
headers; there are no Linux-only calls. The portability rule set (PORT-*,
rules 21-27) enforces these same guards in the code being analyzed, making
the analyzer self-consistent with the portability standards it measures.

Windows is the primary development platform (MinGW-w64 GCC 13 on CLion).
Linux is verified on GCC 13 and Clang. macOS is verified by code inspection;
the same POSIX branch covers all POSIX-compliant systems.

### 4.6 Honesty About Analysis Depth

Metis Code Analyzer Plus uses line-level regex matching for its rule catalog.
This is deliberately honest: the README, CONFIGURATION, and GOVERNMENT
REFERENCES documents all state clearly that the tool is a heuristic signal
generator, not a certified MISRA-C checker or a path-sensitive data-flow
analyzer. False positives are tracked and eliminated (see v2.5.2/v2.5.3
rule-engine fixes) but cannot be eliminated completely with pure regex.

The issue lifecycle system (open / accepted / wontfix / false_positive) exists
precisely to let teams annotate findings without having to disable rules
globally. Suppression is first-class, not an afterthought.

---

## 5. The Scoring Model

The Total Quality Index (TQI) and six health factor scores are computed after
every scan using a penalty-accumulation model:

1. **Issue density** per KLOC is computed for each severity level (critical,
   major, minor, info). A weighted sum (weights in `score.weight.*`) gives a
   single density figure per factor.

2. **Factor score** starts at 100 and is reduced by (density * scale), capped
   at `score.density_cap` (default 60 points maximum reduction from density).

3. **Complexity penalty** further reduces Robustness and Changeability when
   average per-function cyclomatic complexity exceeds `score.complexity.threshold`.

4. **Comment penalty** reduces Transferability and Changeability when the
   comment ratio falls below `score.comment.min_ratio`.

5. **Duplication penalty** reduces Changeability and Transferability
   proportional to the number of duplicate blocks per KLOC.

6. **TQI** is the arithmetic mean of the six factor scores.

7. **Grade** maps TQI (or a factor score) to a letter: A >= 90, B >= 80,
   C >= 65, D >= 50, F otherwise. Thresholds are configurable in `score.grade.*`.

All coefficients are tunable in `code_analyzer.pson`. The defaults represent
a reasonable baseline for mixed-language commercial software; embedded,
safety-critical, or research codebases should adjust them.

---

## 6. Infrastructure Roadmap (GPU, Kubernetes, Containers)

The v2.5.0 release added detection probes for three infrastructure subsystems:

- **GPU** (`GET /api/gpu`): probes for NVIDIA GPU via `nvidia-smi` and for
  OpenCL via the runtime library. Planned: GPU-accelerated duplicate detection
  and metrics using CUDA/OpenCL (C++20; Windows/Linux/macOS; no Docker).

- **Kubernetes** (`GET /api/kubernetes`): probes for kubeconfig and in-cluster
  service account token. Planned: workload metadata scanning (scan running
  container images, pull scan results from a cluster) (C++20; no Kubectl
  dependency).

- **Containers** (`GET /api/containers`): probes for Docker/Podman/containerd
  sockets. Planned: container image layer scanning, Dockerfile analysis
  (C++20; no Docker runtime dependency at build time).

All three are documented in `code_analyzer.pson` with `*.enabled` and `*.note`
keys. The probes are fully operational; the accelerated analysis features are
reserved for future releases. The implementation targets C++20 standard library
plus platform sockets on all three operating systems with no Docker/container
runtime required to build or run the server.

---

## 7. Security Architecture

**Transport**: TLS 1.2 minimum (1.3 recommended). Post-quantum hybrid key
exchange via X25519MLKEM768 (X25519 + ML-KEM-768, NIST FIPS 203) when
OpenSSL 3.5 or later is linked. Classical groups (secp256r1, secp384r1) are
offered as fallback for older clients.

**Authentication**: SHA-256 hashed credentials stored in PSON; no plaintext
passwords. Session tokens are 48-character random strings (configurable). Login
brute-force is throttled by a sliding-window counter per username.

**API Key**: An optional X-API-Key header mechanism for CI/headless use. The
key hash is stored in PSON alongside the session credentials.

**Input validation**: All REST endpoints validate inputs strictly before use.
The directory browser confines paths within the scan root (no traversal). The
static file server confines responses within `server.static_dir`.

**Cryptographic operations**: SHA-256 (password and key hashing), AES-256-GCM
(quarantine encryption in the AV Guard product). ML-KEM-768 (post-quantum key
encapsulation in TLS). All via OpenSSL; no home-grown cryptography.

**Rule scanning**: Lines longer than `analysis.max_rule_line_length` skip regex
matching to prevent catastrophic backtracking on minified files (a denial-of-
service vector for poorly-bounded regexes).

---

## 8. Relationship to Named Products

Metis Code Analyzer Plus is one product in the Metis Suite, a family of
specialized server applications built on the same conventions. Other Metis
Suite products include: Metis Medical Plus (EHR/FHIR platform), Metis AV Guard
with AI (antivirus/EDR), Metis Church Plus (ChMS), Metis Logistics Plus,
Metis Risk Management Platform, Metis Genie Platform, Metis HLASM Studio,
Metis Financial Analyzer Plus, and Metis Security Suite.

All suite products share: pure C++20, PSON configuration, 7-bit ASCII source,
exe-relative paths, zero Docker/Doxygen/GTest/PyTest/Jupyter, MIT license, and
CMake/Ninja builds targeting Windows, Linux, and macOS.

---

*Version 2.7.4 -- Metis Code Analyzer Plus*
*Copyright (c) 2026 Bennie Shearer (Retired). MIT License.*
