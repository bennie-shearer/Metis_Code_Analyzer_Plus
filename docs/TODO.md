# Metis Code Analyzer Plus - TODO

Open work and completed history, tracked honestly.

## Open

### Infrastructure subsystems (future C++20 work; no Docker)
- [ ] GPU acceleration: C++20 for Windows, Linux, macOS.
      GET /api/gpu exists; returns "planned".
- [ ] Kubernetes integration: C++20 for Windows, Linux, macOS.
      GET /api/kubernetes exists; returns "planned".
- [ ] Container integration: C++20 for Windows, Linux, macOS.
      GET /api/containers exists; returns "planned".

### Analysis depth
- [ ] Full per-language AST analysis via tree-sitter for all 46+ languages
      (today AST is opt-in for C, Python, JavaScript, Go, Java, Rust only).
- [ ] Interprocedural data-flow analysis (long-term; requires libclang or
      equivalent semantic library; no Docker, no PyTest, no GTest).
- [ ] Rule catalog expansion: additional MISRA-C, CERT-C, and CWE coverage.

### Operations
- [ ] Persistent issue status via SQLite (today status is in-memory only;
      restarting the server resets all lifecycle overrides).
- [ ] WebSocket push for live scan progress.
- [ ] SAML / OIDC authentication option.

## Completed

### v2.4.2

- [x] Technology Inventory panel: GET /api/technologies wired in dashboard.
      Groups detected frameworks by category; shows name and file count.
- [x] Quality Gate panel: GET /api/gate wired in dashboard. Shows PASS/FAIL
      badge, reason, TQI, issue count, and critical count vs thresholds.
- [x] Issue Status column in Issues table: POST /api/issue/status called on
      drop-down change. Statuses: open, accepted, wontfix, false_positive.
      Color-coded; status persisted server-side for session lifetime.
- [x] Server health dot in header: GET /api/health polled on load and every
      30 s. Green = online; red = unreachable.
- [x] Admin Tools panel: Password Hash Generator calls GET /api/hash-password;
      displays SHA-256 result for paste into code_analyzer.pson.
- [x] docs/API.md fully rewritten to cover all 36 server endpoints.
- [x] version.hpp: METIS_CODE_ANALYZER_VERSION_MAJOR/MINOR/PATCH macros
      corrected to 2.4.2.
- [x] Version 2.4.2 in all references.

### v2.2.0

- [x] Infrastructure panel: GPU, Kubernetes, Container cards (pgpu, pk8s, pctr).
- [x] TLS/Security Configuration panel populated from /api/security.
- [x] AI Configuration panel populated from /api/config ai sub-object.
- [x] /api/security extended with protocol and cert_cn fields.
- [x] /api/config extended with full ai sub-object.
- [x] Port 8080 configurable; BACKGROUND.md re-crafted with ToC.
- [x] All .md/.txt docs consolidated in docs/.

### v2.1.x and earlier

- [x] Export downloads named from scanned project stem (v2.1.7).
- [x] CSV provenance preamble (v2.1.6).
- [x] Rule false-positive tightening: SEC-EVAL, SEC-HARDCODED-SECRET,
      SEC-SYSTEM-CALL, CHG-MAGIC-PATH (v2.1.5).
- [x] Portability rules guard-aware (v2.1.5).
- [x] 46-language support (v2.0.0 + v1.9.6 + v1.8.5).
- [x] AST-accurate metrics via vendored tree-sitter for C, Python,
      JavaScript, Go, Java, Rust (METIS_ENABLE_AST=ON) (v1.9.9).
- [x] Quality gate: GET /api/gate; gate.* PSON; exit_nonzero for CI (v1.9.9).
- [x] Issue lifecycle: POST /api/issue/status (v1.9.9).
- [x] Technology inventory: GET /api/technologies (v1.9.9).
- [x] Post-quantum TLS X25519MLKEM768; OpenSSL 4.0.0 prebuilt (v1.9.7).
- [x] Multi-project support (v1.8.7).
- [x] Incremental scanning: mtime cache (v1.8.7).
- [x] License header detection: GET /api/licenses (v1.8.6).
- [x] Webhook on scan completion (v1.8.6).
- [x] Watch mode: scan.watch (v1.8.6).
- [x] API key authentication: X-API-Key header (v1.8.6).
- [x] PDF, JSON, CSV exports (v1.8.0 / v1.8.6).
- [x] Prometheus metrics: GET /metrics (v1.8.0).
- [x] Trend lines and snapshot persistence (v1.8.0).
- [x] Dependency graph; duplicate block detection (v1.8.0).

## Self-Scan

Current result on scan.root = ".":

  Grade A   TQI 94.9   Issues 0   Duplicate blocks 6
  Security 100   Robustness 100   Efficiency 100
  Transferability 88   Changeability 87
  Files 3   avg_cx 2.3
