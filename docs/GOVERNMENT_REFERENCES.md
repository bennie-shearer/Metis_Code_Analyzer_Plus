# Government and Standards References - Metis Code Analyzer Plus

This document lists the government, government-sponsored, and open-standard
references the product actually relies on, where each is used, and a link to
the authoritative source. Entries are citations only; no specification text is
reproduced here. Refer to each issuing body for the normative document.

## U.S. Government - NIST (National Institute of Standards and Technology)

- **FIPS 180-4, Secure Hash Standard (SHS).**
  Used for: SHA-256 password hashing in the admin authentication path
  (`include/metis/codeanalyzer/auth.hpp`).
  Source: https://csrc.nist.gov/pubs/fips/180-4/upd1/final

- **FIPS 203, Module-Lattice-Based Key-Encapsulation Mechanism (ML-KEM).**
  Used for: the post-quantum key-encapsulation in the TLS hybrid key exchange
  (`X25519MLKEM768`) when the server is built with TLS and linked against an
  OpenSSL that implements ML-KEM (OpenSSL 3.5 or newer). On older OpenSSL the
  server falls back to classical groups and reports that truthfully.
  Source: https://csrc.nist.gov/pubs/fips/203/final

## Government-Sponsored - MITRE

- **CWE (Common Weakness Enumeration).**
  Used for: classifying rule findings. The catalog references CWE-78, CWE-89,
  CWE-95, CWE-327, CWE-330, CWE-798, and CWE-1069
  (`include/metis/codeanalyzer/rules.hpp` and `code_analyzer.pson`).
  CWE is operated by MITRE under U.S. government sponsorship.
  Source: https://cwe.mitre.org/

## Open Standards - IETF (not government, included for completeness)

- **RFC 8446, TLS 1.3.** The TLS layer negotiates TLS 1.3 (minimum TLS 1.2).
  Source: https://www.rfc-editor.org/rfc/rfc8446

- **Hybrid key exchange for TLS 1.3 (X25519MLKEM768 group).** The post-quantum
  hybrid group combines X25519 with ML-KEM-768, per the IETF work on
  ECDHE + ML-KEM hybrids (draft-kwiatkowski-tls-ecdhe-mlkem and related).
  Source: https://datatracker.ietf.org/doc/draft-kwiatkowski-tls-ecdhe-mlkem/

- **RFC 8259, JSON.** API request and response payloads.
  Source: https://www.rfc-editor.org/rfc/rfc8259

- **RFC 9110 / RFC 9112, HTTP Semantics / HTTP/1.1.** The embedded HTTP server.
  Source: https://www.rfc-editor.org/rfc/rfc9112

## Other Standards

- **ISO 32000-1, PDF 1.4 (originally an Adobe specification).** The PDF report
  writer emits PDF 1.4 (`include/metis/codeanalyzer/pdf.hpp`).
  Source: https://www.iso.org/standard/51502.html

- **Prometheus text exposition format.** A community (non-government) de facto
  standard used by the `/metrics` endpoint.
  Source: https://prometheus.io/docs/instrumenting/exposition_formats/

## Notes

- NIST and MITRE references above are the genuinely governmental or
  government-sponsored items. IETF RFCs and the ISO/Adobe and Prometheus items
  are open or industry standards, listed so the full provenance is visible.
- The actual cryptographic implementations are provided by OpenSSL (see
  `THIRD_PARTY.md`); this product does not implement ML-KEM or TLS primitives
  itself.
