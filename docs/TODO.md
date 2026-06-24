# Metis Code Analyzer Plus - TODO

Open work and completed history, tracked honestly.

## Open

All previously open items were completed in v2.5.0. The following items
represent the current forward roadmap (C++20; no Docker, no GTest, no PyTest,
no Doxygen, no Jupyter):

- [ ] SAML / OIDC authentication: bearer-token validation via OIDC discovery
      document and JWK set (pure C++20, TLS required). Planned for v2.6.0.
- [ ] Persistent issue status via SQLite: replace NDJSON persistence with a
      bundled SQLite3 single-file library for indexed queries. Planned v2.6.0.
- [ ] WebSocket per-file progress events: emit {"event":"progress","file":"..."}
      from inside the analyzer.analyze() call for streaming file-by-file updates.
- [ ] Full per-language AST analysis via tree-sitter for all 46+ languages
      (today AST opt-in covers C, Python, JavaScript, Go, Java, Rust).
- [ ] Interprocedural data-flow analysis (long-term; no Docker, no GTest).

## Completed

### v2.5.0

- [x] Persistent issue status. Issue lifecycle overrides (open, accepted,
      wontfix, false_positive) written to data/issue_status.ndjson on every
      POST /api/issue/status change and reloaded at startup. Survives restarts.
      Configurable via ui.issue_status_file in code_analyzer.pson.
- [x] WebSocket push for live scan progress. RFC 6455 WebSocket support added
      to http.hpp (SHA-1 handshake, Base64, text/close frames, send_all).
      ws_route() API on Server. GET /api/scan/ws endpoint. Dashboard Analyze
      button uses WebSocket with POST fallback. Progress spinner in header.
- [x] Infrastructure probing (GPU, Kubernetes, Containers). Real C++20 system
      probing on Windows, Linux, and macOS:
        GPU: probes nvidia-smi, then OpenCL runtime library.
        Kubernetes: checks KUBECONFIG, ~/.kube/config, in-cluster service token.
        Containers: probes Docker/Podman/containerd socket or Windows named pipe.
      Returns available/unavailable/disabled with a detail note. No Docker.
- [x] Rule catalog expansion. 8 new rules added (28-35):
        SEC-XSS-INNERHTML (CWE-79), SEC-PATH-TRAVERSAL (CWE-22),
        SEC-PRINTF-FORMAT (CWE-134), ROB-NULL-DEREF (CWE-476),
        EFF-LOOP-INVARIANT, TRN-COMMENTED-CODE,
        MISRA-C-14-4 (opt-in), CERT-MEM35-C (opt-in).
      Total rules: 35 (27 enabled by default).

### v2.4.x

- [x] Header brand text no longer wraps (white-space:nowrap, flex-shrink:0).
- [x] Console 401 error on /api/projects at page load eliminated.
- [x] openapi.yaml: version corrected (2.1.4 -> 2.4.0); 12 missing endpoint
      specs added covering all 36 server routes.
- [x] docs/ARCHITECTURE.md fully rewritten; corrected include paths.
- [x] docs/CONFIGURATION.md: 9 missing PSON key sections added.
- [x] TLS/AES/PQC badge bar changed to horizontal row layout.
- [x] /api/projects 401 console error fixed; moved to post-auth call.

### v2.3.x

- [x] Technology Inventory panel wired in dashboard (GET /api/technologies).
- [x] Quality Gate panel wired in dashboard (GET /api/gate).
- [x] Issue Status lifecycle column in Issues table (GET+POST /api/issue/status).
- [x] Server health dot in header (GET /api/health, 30 s poll).
- [x] Admin Tools / Password Hash Generator (GET /api/hash-password).
- [x] SEC-SQL-CONCAT false positive fixed (CWE-89 pattern tightened).
- [x] TLS 1.3 badge added; badges changed to horizontal row.
- [x] All .md/.txt files moved to docs/; root has README.md and CHANGELOG.md.

### v2.2.0

- [x] Infrastructure panel with GPU/K8s/Container cards.
- [x] TLS/Security Configuration panel.
- [x] AI Configuration panel.
- [x] /api/security extended with protocol and cert_cn fields.
- [x] /api/config extended with full ai sub-object.
- [x] BACKGROUND.md re-crafted with Table of Contents.

### v2.1.x and earlier

- [x] 46-language support, AST via tree-sitter (C/Python/JS/Go/Java/Rust).
- [x] Quality gate: GET /api/gate; gate.* PSON; exit_nonzero for CI.
- [x] Post-quantum TLS X25519MLKEM768; OpenSSL 4.0.0 prebuilt.
- [x] Multi-project support; incremental scanning; mtime cache.
- [x] License header detection, webhook, watch mode, API key auth.
- [x] PDF, JSON, CSV exports; Prometheus metrics; trend lines.
- [x] Dependency graph; duplicate block detection.
- [x] Rule false-positive tightening (SEC-EVAL, SEC-HARDCODED-SECRET,
      SEC-SYSTEM-CALL, CHG-MAGIC-PATH, PORT-* guard-aware).
- [x] Export downloads named from scanned project stem.
