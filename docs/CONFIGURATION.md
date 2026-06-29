# Metis Code Analyzer Plus - Configuration Reference

Version 2.6.0

All operational settings live in `code_analyzer.pson`. The server loads the
path given as its first argument, or `code_analyzer.pson` in the working
directory if none is given. Nothing operational is hardcoded; every parameter
below has a documented key in the PSON file.

## PSON Format

```
key.path = "string value"
key.path = 8443
key.path = 3.14
key.path = true
key.path = ["a", "b", "c"]
# comment to end of line
```

Embedded double-quote in a string value: write `\"`.

---

## server

| Key                     | Type   | Default        | Meaning                                     |
|-------------------------|--------|----------------|---------------------------------------------|
| `server.host`           | string | `"0.0.0.0"`   | Bind address. `0.0.0.0` binds all NICs.     |
| `port`                  | int    | `8443`         | TCP listen port (HTTP and HTTPS share it).  |
| `server.static_dir`     | string | `"web"`        | Dashboard web root directory.               |
| `server.index_doc`      | string | `"index.html"` | File returned for GET /.                    |
| `server.max_header_bytes`| int   | `1048576`      | Max request header size (DoS guard).        |
| `server.listen_backlog` | int    | `64`           | Kernel accept() queue depth.                |
| `server.recv_chunk_bytes`| int   | `8192`         | Per-call socket read buffer size.           |

---

## scan

| Key                          | Type     | Default | Meaning                                      |
|------------------------------|----------|---------|----------------------------------------------|
| `scan.root`                  | string   | `"."`   | Directory scanned by default.                |
| `scan.on_start`              | bool     | `true`  | Run a scan when the server starts.           |
| `scan.exclude`               | string[] | see cfg | Base names skipped during the recursive walk.|
| `scan.watch`                 | bool     | `false` | Poll scan root and rescan on file changes.   |
| `scan.watch_interval_seconds`| int      | `30`    | Polling interval for watch mode.             |

---

## debt

| Key                           | Type   | Default | Meaning                                        |
|-------------------------------|--------|---------|------------------------------------------------|
| `debt.hourly_rate`            | double | `75.0`  | Developer rate for cost estimates (USD/hour).  |
| `debt.dev_minutes_per_line`   | double | `0.5`   | Dev effort per line; anchors the debt ratio.   |

---

## score

| Key                                    | Type   | Default | Meaning                                          |
|----------------------------------------|--------|---------|--------------------------------------------------|
| `score.weight.critical`                | double | `10.0`  | Critical issue weight in density.                |
| `score.weight.major`                   | double | `5.0`   | Major issue weight.                              |
| `score.weight.minor`                   | double | `2.0`   | Minor issue weight.                              |
| `score.weight.info`                    | double | `1.0`   | Info issue weight.                               |
| `score.scale.security`                 | double | `12.0`  | Security penalty scale per weighted issue/KLOC.  |
| `score.scale.robustness`               | double | `10.0`  | Robustness penalty scale.                        |
| `score.scale.efficiency`               | double | `8.0`   | Efficiency penalty scale.                        |
| `score.scale.transferability`          | double | `6.0`   | Transferability penalty scale.                   |
| `score.scale.changeability`            | double | `6.0`   | Changeability penalty scale.                     |
| `score.scale.portability`              | double | `8.0`   | Portability penalty scale.                       |
| `score.density_cap`                    | double | `60.0`  | Max density penalty per factor.                  |
| `score.grade.a`                        | double | `90.0`  | Minimum score for grade A.                       |
| `score.grade.b`                        | double | `80.0`  | Minimum score for grade B.                       |
| `score.grade.c`                        | double | `65.0`  | Minimum score for grade C.                       |
| `score.grade.d`                        | double | `50.0`  | Minimum score for grade D.                       |
| `score.complexity.threshold`           | double | `5.0`   | Per-function complexity limit (global).          |
| `score.complexity.threshold.<lang>`    | double | varies  | Per-language override.                           |
| `score.complexity.robustness_mult`     | double | `1.5`   | Robustness penalty per excess complexity point.  |
| `score.complexity.robustness_cap`      | double | `25.0`  | Cap on complexity-driven Robustness penalty.     |
| `score.complexity.changeability_mult`  | double | `1.2`   | Changeability penalty per excess complexity.     |
| `score.complexity.changeability_cap`   | double | `20.0`  | Cap on complexity-driven Changeability penalty.  |
| `score.comment.min_ratio`              | double | `0.10`  | Minimum comment/total-lines ratio.               |
| `score.comment.transferability_mult`   | double | `1.5`   | Transferability penalty per ratio-point below min.|
| `score.comment.transferability_cap`    | double | `20.0`  | Cap on comment-driven Transferability penalty.   |
| `score.comment.changeability_mult`     | double | `1.0`   | Changeability penalty.                           |
| `score.comment.changeability_cap`      | double | `15.0`  | Cap on comment-driven Changeability penalty.     |
| `score.duplication.changeability_mult` | double | `2.0`   | Changeability penalty per duplicate block/KLOC.  |
| `score.duplication.changeability_cap`  | double | `20.0`  | Cap on duplication-driven Changeability penalty. |
| `score.duplication.transferability_mult`| double| `1.5`  | Transferability penalty per duplicate block/KLOC.|
| `score.duplication.transferability_cap`| double | `15.0` | Cap on duplication-driven Transferability.       |

---

## analysis

| Key                              | Type   | Default   | Meaning                                            |
|----------------------------------|--------|-----------|----------------------------------------------------|
| `analysis.max_file_bytes`        | int    | `4194304` | Per-file read cap (files larger are skipped).      |
| `analysis.duplication_block_lines`| int   | `6`       | Sliding-window size for duplicate hashing.         |
| `analysis.long_line_columns`     | int    | `120`     | Column count marking a line as "long".             |
| `analysis.max_rule_line_length`  | int    | `2000`    | Lines longer skip regex matching (backtrack guard).|
| `analysis.incremental`           | bool   | `true`    | Reuse cached metrics for unmodified files.         |

---

## auth

| Key                       | Type   | Default   | Meaning                                                |
|---------------------------|--------|-----------|--------------------------------------------------------|
| `auth.enabled`            | bool   | `true`    | Require sign-in for data and control endpoints.        |
| `auth.username`           | string | `"admin"` | Administrator username.                                |
| `auth.password_sha256`    | string | (default) | SHA-256 hex of password. Default: Admin#2026.          |
| `auth.session_minutes`    | int    | `60`      | Session lifetime in minutes.                           |
| `auth.session_token_length`| int   | `48`      | Session token length (characters).                     |
| `auth.login_max_attempts` | int    | `5`       | Failed logins per username per window (0 = disabled).  |
| `auth.login_window_seconds`| int   | `60`      | Sliding window for login_max_attempts.                 |
| `auth.api_key_enabled`    | bool   | `false`   | Allow X-API-Key header authentication.                 |
| `auth.api_key_sha256`     | string | `""`      | SHA-256 hex of API key. Keep secret.                   |

**Generate a password hash**: `POST /api/hash-password` with body
`{"value":"<password>"}` or `GET /api/hash-password?value=<password>`.

**Change the password**: update `auth.password_sha256` in PSON and restart.

---

## log

| Key                 | Type   | Default    | Meaning                                          |
|---------------------|--------|------------|--------------------------------------------------|
| `log.level`         | string | `"info"`   | Minimum level: `debug`, `info`, `warn`, `error`. |
| `log.max_entries`   | int    | `1000`     | Ring-buffer capacity; oldest dropped when full.  |
| `log.default_limit` | int    | `200`      | Default entries returned by GET /api/logs.       |

---

## ui

| Key                    | Type   | Default | Meaning                                              |
|------------------------|--------|---------|------------------------------------------------------|
| `ui.max_table_rows`    | int    | `500`   | Max rows in Issues and Files tables.                 |
| `ui.pdf_max_issues`    | int    | `40`    | Max issues in the PDF quality report.                |
| `ui.history_max`       | int    | `50`    | Scan snapshots kept for the Trend panel.             |
| `ui.history_file`      | string | `""`    | NDJSON path for persistent scan history.             |
| `ui.issue_status_file` | string | `""`    | NDJSON path for persistent issue status overrides.   |

---

## i18n

| Key                   | Type     | Default    | Meaning                                   |
|-----------------------|----------|------------|-------------------------------------------|
| `i18n.default_locale` | string   | `"en-US"`  | Locale selected on first browser load.    |
| `i18n.locales`        | string[] | eight set  | Locales in the language drop-down.        |

Supported locales: `en-US`, `en-GB`, `fr-FR`, `de-DE`, `es-ES`, `it-IT`,
`pt-BR`, `ja-JP`. Translations live in `web/i18n.js`.

---

## tls

| Key              | Type   | Default                           | Meaning                                          |
|------------------|--------|-----------------------------------|--------------------------------------------------|
| `tls.enabled`    | bool   | `true`                            | Terminate TLS on the listen port.                |
| `tls.cert_file`  | string | `""`                              | PEM cert path. Empty = auto-generate self-signed.|
| `tls.key_file`   | string | `""`                              | PEM key path. Empty = auto-generate self-signed. |
| `tls.groups`     | string | `"X25519MLKEM768:X25519:..."`     | TLS key-exchange groups (colon-separated).       |
| `tls.min_version`| string | `"1.2"`                           | Minimum TLS version: `"1.2"` or `"1.3"`.        |
| `tls.cert_days`  | int    | `365`                             | Validity days for auto-generated cert.           |
| `tls.key_bits`   | int    | `2048`                            | RSA key size for auto-generated cert.            |
| `tls.cert_cn`    | string | `"Metis Code Analyzer Plus"`      | CN for auto-generated cert.                      |

**Windows cert generation**: run
`powershell -ExecutionPolicy Bypass -File certs\gen_certs.ps1`
then set `tls.cert_file` and `tls.key_file` to the generated paths.

---

## ai

| Key                    | Type   | Default | Meaning                                          |
|------------------------|--------|---------|--------------------------------------------------|
| `ai.enabled`           | bool   | `false` | Allow POST /api/ai/summarize.                    |
| `ai.provider`          | string | `""`    | Label shown in AI Configuration panel.           |
| `ai.endpoint`          | string | `""`    | Full HTTPS URL of the provider API.              |
| `ai.model`             | string | `""`    | Model identifier sent in the request.            |
| `ai.api_key`           | string | `""`    | Provider API key (Bearer token). Keep secret.    |
| `ai.timeout_seconds`   | int    | `30`    | Outbound request timeout.                        |
| `ai.max_output_tokens` | int    | `1024`  | Token cap on the model response.                 |
| `ai.max_report_issues` | int    | `15`    | Issues included in the AI summary prompt.        |
| `ai.system_prompt`     | string | see cfg | System instruction for every summary request.    |

---

## webhook

| Key                      | Type   | Default | Meaning                                        |
|--------------------------|--------|---------|------------------------------------------------|
| `webhook.enabled`        | bool   | `false` | Fire a POST after each scan.                   |
| `webhook.url`            | string | `""`    | Destination URL.                               |
| `webhook.on_grade_below` | string | `""`    | Only fire when grade is below this threshold.  |
| `webhook.timeout_seconds`| int    | `10`    | Outbound timeout.                              |

---

## gate

| Key                        | Type   | Default | Meaning                                         |
|----------------------------|--------|---------|-------------------------------------------------|
| `gate.enabled`             | bool   | `false` | Evaluate gate after each scan.                  |
| `gate.min_tqi`             | double | `80.0`  | Minimum TQI required to pass (0 = disabled).    |
| `gate.max_issues`          | int    | `-1`    | Maximum total issues allowed (-1 = disabled).   |
| `gate.max_critical`        | int    | `0`     | Maximum critical-severity issues (-1 = disabled).|
| `gate.fail_on_new_critical`| bool   | `false` | Fail if new critical issues appear vs. prior scan.|
| `gate.exit_nonzero`        | bool   | `false` | Exit with code 1 on gate failure (CI use).      |

---

## technologies

| Key                    | Type | Default | Meaning                                             |
|------------------------|------|---------|-----------------------------------------------------|
| `technologies.enabled` | bool | `true`  | Detect frameworks and libraries from import patterns.|

---

## gpu / kubernetes / containers

| Key                  | Type   | Default | Meaning                                               |
|----------------------|--------|---------|-------------------------------------------------------|
| `gpu.enabled`        | bool   | `false` | Probe for GPU runtimes.                               |
| `gpu.note`           | string | see cfg | Note returned when `gpu.enabled = false`.             |
| `kubernetes.enabled` | bool   | `false` | Probe for Kubernetes kubeconfig or in-cluster token.  |
| `kubernetes.note`    | string | see cfg | Note returned when disabled.                          |
| `containers.enabled` | bool   | `false` | Probe for Docker/Podman/containerd sockets.           |
| `containers.note`    | string | see cfg | Note returned when disabled.                          |

---

## project (multi-project)

```
project.1.name = "Server"
project.1.root = "./server"
project.2.name = "Client"
project.2.root = "./client"
```

When no `project.N` entries are present, one implicit project named `"default"`
is created using `scan.root`. Switch via `POST /api/project/select {"name":"..."}`.

---

## Rule Catalog Keys

Each rule has up to eight keys:

| Sub-key   | Type   | Required | Meaning                                           |
|-----------|--------|----------|---------------------------------------------------|
| `id`      | string | yes      | Unique rule ID (shown in reports and exports).    |
| `title`   | string | yes      | One-line description.                             |
| `cwe`     | string | no       | CWE reference or `""`.                            |
| `severity`| string | yes      | `critical` / `major` / `minor` / `info`.         |
| `factor`  | string | yes      | Health factor affected.                           |
| `minutes` | int    | yes      | Estimated remediation effort (minutes).           |
| `enabled` | bool   | no       | Default `true`. Set `false` to disable.           |
| `pattern` | string | yes      | ECMAScript regex (case-insensitive per line).     |

Rules are loaded in numeric order (`rule.1` through first gap). When any
`rule.N.*` keys exist, built-in `default_rules()` is not used.
