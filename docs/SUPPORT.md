# Support - Metis Code Analyzer Plus

Version 2.7.4

How to get help with Metis Code Analyzer Plus, report problems, and disclose
security issues.

## Before Reaching Out

1. Confirm your version: `GET /api/version` or check the dashboard header.
2. Read `README.md`, `docs/CONFIGURATION.md`, and `docs/API.md`.
3. Reproduce with a default `code_analyzer.pson` to rule out config drift.
4. Collect the activity log (dashboard Activity Log panel -> Download, or
   `GET /api/logs`) and the exact request / response involved.

## Generating a New Admin Password

1. Start the server.
2. Send: `POST /api/hash-password` with body `{"value":"<new-password>"}`
   (or `GET /api/hash-password?value=<new-password>` -- encode `#` as `%23`
   in GET URLs to prevent fragment truncation).
3. Copy the returned `"hash"` value.
4. Set `auth.password_sha256 = "<hash>"` in `code_analyzer.pson`.
5. Restart the server.

## Reporting a Problem

Open an issue with:

- Product and version (e.g. Metis Code Analyzer Plus 2.7.4).
- Operating system and compiler (e.g. Windows, MinGW-w64 GCC 13).
- The scan root and a description of the codebase size.
- Steps to reproduce, expected vs. actual behavior.
- Relevant log lines and any error output from `GET /api/logs`.

## Support Channels

- Primary maintainer: Bennie Shearer (Retired)
- Issue tracker: (add repository issues URL)
- Security disclosure: (add private contact)

## Security Disclosure

Do not file public issues for security vulnerabilities. Report them privately
to the maintainer with reproduction details and impact assessment. Note that
static analysis runs against source you point it at; treat scan roots and
report output as potentially sensitive.

## Service Expectations

This software is delivered as-is under the MIT License (see `LICENSE`).
Response times, maintenance windows, and guarantees are whatever your own
deployment agreement specifies. None are implied by this document.
