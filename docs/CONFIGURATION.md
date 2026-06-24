# Metis Code Analyzer Plus - Configuration

All operational settings live in `code_analyzer.pson`. The server loads the path given
as its first argument, or `code_analyzer.pson` in the working directory if none is
given. Nothing operational is hardcoded.

## Format

PSON here uses dotted keys with typed scalars and arrays. A `#` outside a
quoted string begins a comment.

```
key.path = "string value"
key.path = 8080
key.path = 3.14
key.path = true
key.path = ["a", "b", "c"]
```

Note: this follows the common Metis dotted-key PSON convention. If the suite's
canonical PSON parser differs, reconcile `pson.hpp` and this file before
packaging.

## Keys

### server

| Key                  | Type   | Default     | Meaning                                  |
|----------------------|--------|-------------|------------------------------------------|
| `server.host`        | string | `"0.0.0.0"` | Bind address. `0.0.0.0` binds all NICs.  |
| `server.port`        | int    | `8080`      | TCP listen port.                         |
| `server.static_dir`  | string | `"web"`     | Directory served as the dashboard root.  |

### scan

| Key               | Type     | Default | Meaning                                       |
|-------------------|----------|---------|-----------------------------------------------|
| `scan.root`       | string   | `"."`   | Directory analyzed on startup and by default. |
| `scan.on_start`   | bool     | `true`  | Run a scan when the server starts.            |
| `scan.exclude`    | string[] | see cfg | Directory and file names skipped during walk. |

### debt

| Key                          | Type   | Default | Meaning                                            |
|------------------------------|--------|---------|----------------------------------------------------|
| `debt.hourly_rate`           | double | `75.0`  | Blended remediation rate for cost estimates.       |
| `debt.dev_minutes_per_line`  | double | `0.5`   | Development-cost basis for the debt ratio.         |

### auth

| Key                     | Type   | Default   | Meaning                                                  |
|-------------------------|--------|-----------|----------------------------------------------------------|
| `auth.enabled`          | bool   | `true`    | Require admin sign-in for data and control endpoints.    |
| `auth.username`         | string | `"admin"` | Administrator username.                                  |
| `auth.password_sha256`  | string | (none)    | SHA-256 hex of the password. No plaintext is stored.     |
| `auth.session_minutes`  | int    | `60`      | Session lifetime; refreshed on each authorized request.  |

Default credentials are `admin` / `Admin#2026`. Change them before deployment:
compute a new SHA-256 hash of your password and set `auth.password_sha256`. If
`auth.enabled` is true but the hash is empty, all logins are rejected.

### log

| Key               | Type   | Default  | Meaning                                            |
|-------------------|--------|----------|----------------------------------------------------|
| `log.level`       | string | `"info"` | Minimum level: `debug`, `info`, `warn`, `error`.   |
| `log.max_entries` | int    | `1000`   | In-memory ring-buffer capacity; oldest are dropped.|

### i18n

| Key                   | Type     | Default    | Meaning                                |
|-----------------------|----------|------------|----------------------------------------|
| `i18n.default_locale` | string   | `"en-US"`  | Locale selected on first load.         |
| `i18n.locales`        | string[] | eight set  | Locales offered in the language menu.  |

Shipped locales: `en-US`, `en-GB`, `fr-FR`, `de-DE`, `es-ES`, `it-IT`,
`pt-BR`, `ja-JP`. Translations live in `web/i18n.js`.

## Example

```
server.host = "0.0.0.0"
server.port = 8080
server.static_dir = "web"

scan.root = "."
scan.on_start = true
scan.exclude = [".git", "build", "node_modules", "dist", "vendor"]

debt.hourly_rate = 75.0
debt.dev_minutes_per_line = 0.5
```

## Notes

- Changing the scan root at runtime does not require a config edit: pass
  `{ "root": "..." }` to `POST /api/scan`, or use the dashboard's root field.
- The exclude list matches plain directory and file names, not glob patterns,
  in this release.

### score (scoring model)

All scoring tunables, with the baseline defaults:

- Severity weights: `score.weight.critical` (10), `score.weight.major` (5),
  `score.weight.minor` (2), `score.weight.info` (1).
- Per-factor density scales (penalty per weighted point per KLOC):
  `score.scale.security` (12), `score.scale.robustness` (10),
  `score.scale.efficiency` (8), `score.scale.transferability` (6),
  `score.scale.changeability` (6); `score.density_cap` (60).
- Grade thresholds: `score.grade.a` (90), `.b` (80), `.c` (65), `.d` (50);
  below `d` is F. These are also sent to the dashboard so it grades identically.
- Complexity penalties: `score.complexity.threshold` (10) plus
  `.robustness_mult`/`.robustness_cap` and
  `.changeability_mult`/`.changeability_cap`.
- Comment-ratio penalties: `score.comment.min_ratio` (0.10) plus the
  transferability and changeability mult/cap pairs.
- Duplication penalties: `score.duplication.*` mult/cap pairs.

### analysis (sizing)

| Key                                | Type | Default   | Meaning                                  |
|------------------------------------|------|-----------|------------------------------------------|
| `analysis.max_file_bytes`          | int  | `4194304` | Per-file read cap (4 MB).                |
| `analysis.duplication_block_lines` | int  | `6`       | Window size for duplicate-block hashing. |
| `analysis.long_line_columns`       | int  | `120`     | Column count that marks a line as long.  |

### rule (rule catalog)

The rule engine loads `rule.1.*`, `rule.2.*`, ... in order until the first
missing `id`. Each rule has:

| Field      | Meaning                                                              |
|------------|----------------------------------------------------------------------|
| `id`       | Stable rule identifier (e.g. `SEC-WEAK-HASH`).                        |
| `title`    | Human-readable finding description.                                  |
| `cwe`      | CWE reference, or empty.                                             |
| `severity` | `critical`, `major`, `minor`, or `info`.                            |
| `factor`   | Health factor the finding counts against.                            |
| `minutes`  | Estimated remediation effort.                                        |
| `pattern`  | ECMAScript regex, matched case-insensitively per line.               |
| `enabled`  | Optional; set `false` to keep a rule defined but inactive.           |

In pattern strings, write an embedded double-quote as `\"`; all other
backslashes are taken verbatim so regex escapes survive. If no `rule.*` entries
are present, the built-in catalog is used.

| `analysis.max_rule_line_length` | int | `2000` | Lines longer than this are skipped for rule matching (prevents regex backtracking hangs on minified/generated files). |

### tls (post-quantum TLS)

Only effective in a build with `METIS_ENABLE_TLS=ON`.

| Key             | Type   | Default                                      | Meaning                                                        |
|-----------------|--------|----------------------------------------------|----------------------------------------------------------------|
| `tls.enabled`   | bool   | `true`                                       | Serve over TLS on the top-level `port`. A build without TLS support warns and serves plain HTTP instead. |
| `tls.cert_file` | string | `""`                                         | PEM certificate path; empty uses an ephemeral self-signed cert.|
| `tls.key_file`  | string | `""`                                         | PEM private key path.                                          |
| `tls.groups`    | string | `X25519MLKEM768:X25519:secp256r1:secp384r1`  | Key-exchange groups, highest priority first.                   |
| `tls.min_version`| string| `"1.2"`                                      | Minimum TLS protocol version ("1.2" or "1.3").                 |
| `tls.cert_days` | int    | `365`                                        | Validity period of a generated self-signed certificate.        |
| `tls.key_bits`  | int    | `2048`                                       | RSA key size for a generated self-signed certificate.          |
| `tls.cert_cn`   | string | `Metis Code Analyzer Plus`                   | Common name for a generated self-signed certificate.           |

The hybrid `X25519MLKEM768` group provides post-quantum protection (ML-KEM,
NIST FIPS 203) and requires OpenSSL 3.5+. With older OpenSSL the server logs the
fallback and serves classical groups; the banner reports the truth either way.

### ai (optional AI summary)

Outbound HTTPS to an OpenAI-compatible provider. Requires a build with
`METIS_ENABLE_TLS=ON`.

| Key                  | Type   | Default | Meaning                                            |
|----------------------|--------|---------|----------------------------------------------------|
| `ai.enabled`         | bool   | `false` | Allow `POST /api/ai/summarize` to call the provider.|
| `ai.provider`        | string | `""`    | Free-form provider label.                          |
| `ai.endpoint`        | string | `""`    | Full HTTPS chat/completions URL.                   |
| `ai.model`           | string | `""`    | Model identifier sent in the request.              |
| `ai.api_key`         | string | `""`    | Provider API key (sent as a bearer token).         |
| `ai.timeout_seconds` | int    | `30`    | Outbound request timeout.                          |
| `ai.max_output_tokens`| int   | `1024`  | Cap on the model response length.                  |
| `ai.system_prompt`   | string | (set)   | Instruction prepended to the report summary.       |

### gpu / kubernetes / containers (planned)

Reserved for future C++20 implementations (Windows/Linux/macOS). The endpoints
`/api/gpu`, `/api/kubernetes`, and `/api/containers` report "planned" today.

| Key                  | Type   | Default | Meaning                          |
|----------------------|--------|---------|----------------------------------|
| `gpu.enabled`        | bool   | `false` | Reserved.                        |
| `gpu.note`           | string | (set)   | Status note shown in the UI.     |
| `kubernetes.enabled` | bool   | `false` | Reserved.                        |
| `kubernetes.note`    | string | (set)   | Status note shown in the UI.     |
| `containers.enabled` | bool   | `false` | Reserved.                        |
| `containers.note`    | string | (set)   | Status note shown in the UI.     |

Note: the listen port is the top-level `port` key (default 8080) and is used for
both HTTP and HTTPS. There is no separate TLS port.

### gate (quality gate)

Controls the CI/CD quality gate evaluated at startup and via `GET /api/gate`.
Set `gate.exit_nonzero = true` to exit with code 1 on failure for pipeline
integration.

| Key                  | Type   | Default | Meaning                                                      |
|----------------------|--------|---------|--------------------------------------------------------------|
| `gate.enabled`       | bool   | `false` | Enable the quality gate.                                     |
| `gate.min_tqi`       | double | `70.0`  | Minimum acceptable TQI. Gate fails if TQI is below this.    |
| `gate.max_issues`    | int    | `50`    | Maximum total issue count before gate fails.                 |
| `gate.max_critical`  | int    | `0`     | Maximum critical-severity issues before gate fails.          |
| `gate.exit_nonzero`  | bool   | `false` | Exit with code 1 when gate fails (for CI pipelines).        |

### technologies (framework detection)

| Key                    | Type | Default | Meaning                                               |
|------------------------|------|---------|-------------------------------------------------------|
| `technologies.enabled` | bool | `true`  | Detect frameworks and libraries from import patterns. |

### api_key (headless / CI authentication)

Provides an alternative to session-cookie auth for CI pipelines and scripts.

| Key                       | Type   | Default | Meaning                                               |
|---------------------------|--------|---------|-------------------------------------------------------|
| `auth.api_key_enabled`    | bool   | `false` | Accept the `X-API-Key` header for authentication.    |
| `auth.api_key_sha256`     | string | `""`    | SHA-256 hex of the API key. Compute via `/api/hash-password`. |

### webhook (scan completion notification)

Sends an outbound HTTP POST to a URL of your choice when a scan completes.
Requires a build with `METIS_ENABLE_TLS=ON` for HTTPS targets.

| Key                       | Type   | Default | Meaning                                                     |
|---------------------------|--------|---------|-------------------------------------------------------------|
| `webhook.enabled`         | bool   | `false` | Send a POST to `webhook.url` after each scan completes.     |
| `webhook.url`             | string | `""`    | Target URL. HTTP or HTTPS.                                  |
| `webhook.timeout_seconds` | int    | `5`     | Outbound request timeout.                                   |

### scan.watch (automatic re-scan)

| Key                          | Type | Default | Meaning                                                  |
|------------------------------|------|---------|----------------------------------------------------------|
| `scan.watch`                 | bool | `false` | Poll the scan root and re-scan when files change.        |
| `scan.watch_interval_seconds`| int  | `60`    | Polling interval in seconds.                             |

### ui (dashboard tuning)

| Key                  | Type | Default | Meaning                                                        |
|----------------------|------|---------|----------------------------------------------------------------|
| `ui.max_table_rows`  | int  | `500`   | Maximum rows shown in the Issues and Files tables.             |
| `ui.pdf_max_issues`  | int  | `40`    | Maximum issues listed in the exported PDF report.              |
| `ui.history_max`     | int  | `100`   | Number of past scan snapshots retained for trend display.      |
| `ui.history_file`    | string | `""` | Path to a newline-delimited JSON file for persistent history.  |

### arch (architecture rules)

Optional dependency-policy rules. Violations are reported as issues with
severity "major".

| Key                     | Type   | Default | Meaning                                                   |
|-------------------------|--------|---------|-----------------------------------------------------------|
| `arch.enabled`          | bool   | `false` | Enable architecture rule checking.                        |
| `arch.module.N.name`    | string | --      | Name of module group N.                                   |
| `arch.module.N.pattern` | string | --      | File path pattern (regex) for files in module N.          |
| `arch.allow.N.from`     | string | --      | Module that is permitted to import from `arch.allow.N.to`.|
| `arch.allow.N.to`       | string | --      | Target module name.                                       |

### project (multi-project support)

When no `project.N` entries are present, one implicit project named "default"
is created from `scan.root`. With named projects the dashboard shows a
project-selector drop-down instead of the scan-root text input.

| Key                  | Type   | Default | Meaning                           |
|----------------------|--------|---------|-----------------------------------|
| `project.N.name`     | string | --      | Display name for project N.       |
| `project.N.root`     | string | --      | Scan root directory for project N.|
