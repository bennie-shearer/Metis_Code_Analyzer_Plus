# Metis Code Analyzer Plus - TODO

Open work and completed history. No Docker, Doxygen, PyTest, GTest, or
Jupyter in any planned or completed item. C++20 throughout.

---

## Recommended Improvements (Open)

The following items represent the current forward roadmap:

- [ ] **SAML / OIDC authentication** (v2.7.0): bearer-token validation via
      OIDC discovery document and JWK set. Pure C++20, TLS required.
      No external auth library; implements the OIDC subset needed for
      enterprise SSO (discovery, JWKS fetch, JWT RS256 verify).

- [ ] **SQLite issue persistence** (v2.7.0): replace NDJSON persistence
      (`data/issue_status.ndjson`) with bundled SQLite3 single-file library
      for indexed queries. Enables filtering by status, rule, file, or date.
      No Docker; single-file SQLite source dropped into `vendor/sqlite/`.

- [ ] **WebSocket per-file progress events**: emit
      `{"event":"progress","file":"...","n":N,"total":N}` from inside
      `analyzer.analyze()` for streaming file-by-file progress in the dashboard.
      Requires thread-safe per-connection write; redesign WS handler.

- [ ] **Full per-language AST analysis via tree-sitter**: extend AST opt-in
      from the current six languages (C, Python, JS, Go, Java, Rust) to all
      46 supported languages as their tree-sitter grammars stabilize.

- [ ] **GPU-accelerated duplicate detection** (v2.8.0 target): use CUDA or
      OpenCL (whichever is probed available by `GET /api/gpu`) for
      parallel FNV-1a hashing of the duplicate-block sliding window. C++20
      with CUDA C++ or OpenCL 3.0; Windows/Linux/macOS. No Docker.

- [ ] **Kubernetes workload scanning** (v2.8.0 target): when
      `kubernetes.enabled = true` and a cluster is reachable, list running
      workloads and optionally pull source references for scanning. Pure C++20
      HTTP client (reuses http.hpp TLS stack); no kubectl binary dependency.

- [ ] **Container image scanning** (v2.9.0 target): parse Dockerfiles and
      OCI image manifests. Pull layer tarballs via container runtime socket
      (Docker/Podman/containerd). Analyze extracted source layers with the
      existing scanner. C++20; no Docker runtime build dependency.

- [ ] **Interprocedural data-flow analysis** (long-term): taint tracking
      across function call boundaries for the AST-enabled languages. Requires
      a call-graph phase; substantial C++20 implementation work. No GTest;
      self-scan and manual regression testing.

- [ ] **Architecture layer enforcement**: implement the `arch.*` PSON keys
      (module group definitions and allowed import relationships). Violations
      reported as `ARCH-LAYER-VIOLATION` issues. C++20.

- [ ] **Rule registry sync**: periodic HTTP fetch of a hosted rule registry
      to update rule patterns without a binary rebuild. Pulls a signed PSON
      blob via TLS; verifies signature before applying. Opt-in per deployment.

- [ ] **Incremental WebSocket rescan on watch**: when `scan.watch = true`,
      emit per-file progress events over an open WebSocket connection so the
      dashboard updates in real time during background rescans.

---

## Completed

### v2.6.0 (2026-06-28)

- [x] Full PSON inline documentation for every operational parameter.
- [x] Hardcoded paths removed from PSON defaults (scan.root, tls paths).
- [x] Unicode characters replaced with 7-bit ASCII in all shipped files.
- [x] BACKGROUND.md fully rewritten with eight-section Table of Contents.
- [x] README.md cleaned for GitHub with complete API table and operations guide.
- [x] LICENSE updated with dedication, disclaimer, and acknowledgments.
- [x] .gitignore created.
- [x] docs/CHANGELOG.md completed with full version history.
- [x] All version references verified consistent at 2.6.0.

### v2.5.3 (2026-06-28)

- [x] CHG-MAGIC-PATH PSON pattern (`rule.10.pattern`) fixed. v2.5.2 had
      updated `default_rules()` in `rules.hpp` but the running code uses the
      PSON rules. Pattern rebuilt from correct target regex with Python
      round-trip verification.
- [x] `rules.hpp` `default_rules()` CHG-MAGIC-PATH raw string backslash
      count corrected.

### v2.5.2 (2026-06-28)

- [x] CHG-MAGIC-PATH: suppressed false positive on K8s in-cluster SA path
      `/var/run/secrets/kubernetes.io/serviceaccount/token` via negative
      lookahead in the pattern.
- [x] SEC-XSS-INNERHTML: pattern tightened to eliminate 27 false positives
      (empty-string clears, pure literals, all-esc()-wrapped interpolations).
      3 genuine findings retained.

### v2.5.0

- [x] Persistent issue status (NDJSON, survives restarts).
- [x] WebSocket live scan progress (RFC 6455; dashboard spinner).
- [x] Infrastructure probing: GPU, Kubernetes, Containers (C++20, all platforms).
- [x] Rule catalog expanded to 35 rules (27 enabled by default).
- [x] Platform-guard-aware suppression for PORT-*, CHG-MAGIC-PATH,
      SEC-SYSTEM-CALL.

### v2.4.x

- [x] Header brand text nowrap fix.
- [x] /api/projects 401 console error fixed.
- [x] openapi.yaml: version corrected; 12 missing endpoints added.
- [x] docs/ARCHITECTURE.md rewritten.
- [x] docs/CONFIGURATION.md: 9 missing key sections added.
- [x] TLS/AES/PQC badge bar horizontal layout.

### v2.3.x

- [x] Technology Inventory, Quality Gate, Issue Status panels in dashboard.
- [x] Server health dot in header.
- [x] Admin Tools / Password Hash Generator.
- [x] SEC-SQL-CONCAT false positive fix.
- [x] All .md/.txt files moved to docs/.

### v2.2.0

- [x] Infrastructure panel (GPU/K8s/Container cards).
- [x] TLS/Security and AI Configuration panels.
- [x] /api/security and /api/config extended.

### v2.1.x and Earlier

- [x] 46-language support; tree-sitter AST opt-in (6 languages).
- [x] Quality gate with exit_nonzero for CI.
- [x] Post-quantum TLS X25519MLKEM768; OpenSSL 4.0.0 Windows prebuilt.
- [x] Multi-project support; incremental scanning; mtime cache.
- [x] License header detection, webhook, watch mode, API key auth.
- [x] PDF, JSON, CSV exports; Prometheus metrics; trend lines.
- [x] Dependency graph; duplicate block detection.
- [x] Portability rule set (PORT-*; 7 rules; platform-guard suppression).
