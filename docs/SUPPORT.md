# Support - Metis Code Analyzer Plus

How to get help with Metis Code Analyzer Plus, report problems, and disclose
security issues.

## Before reaching out

1. Confirm your version: `GET /api/version` or the dashboard header.
2. Check `README.md`, `docs/CONFIGURATION.md`, and `docs/API.md`.
3. Reproduce with a fresh `code_analyzer.pson` to rule out config drift.
4. Collect the activity log (dashboard Activity Log -> Download, or
   `GET /api/logs`) and the exact request and response involved.

## Reporting a problem

Open an issue with:

- Product and version (e.g. Metis Code Analyzer Plus 2.4.2).
- Operating system and compiler (e.g. Windows, MinGW-w64 GCC 13).
- The scan root and a description of the codebase size.
- Steps to reproduce, expected vs. actual behavior.
- Relevant log lines and any error output.

## Support channels

> Fill in the channels appropriate to your deployment. Suggested structure:

- Primary maintainer contact: <add address>
- Issue tracker: <add repository issues URL>
- Internal integrators / operators: <add distribution list>
- Contractor / on-call escalation: <add channel>

## Security disclosure

Do not file public issues for security vulnerabilities. Report them privately
to the maintainer contact above with reproduction details and impact. Note that
static analysis runs against source you point it at; treat scan roots and report
output as potentially sensitive.

## Service expectations

This is delivered as-is under the MIT license (see `LICENSE`). Response times,
maintenance windows, and any guarantees are whatever your own deployment
agreement specifies; none are implied by this document.
