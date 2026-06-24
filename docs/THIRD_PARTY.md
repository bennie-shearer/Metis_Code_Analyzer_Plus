# Third-Party Components - Metis Code Analyzer Plus

This product vendors the following third-party component. It is included
locally (no CDN) and retains its own license notice.

## Chart.js

- Version: 4.4.1
- File: `web/chart.umd.js`
- Purpose: dashboard charts (severity bar, language breakdown, complexity bars).
- License: MIT. The full license notice is preserved in the banner at the top
  of `web/chart.umd.js`.
- Project: https://www.chartjs.org

No other third-party code is bundled. The server has no external dependencies
in the default build.

## OpenSSL (optional, not bundled)

- Used only when built with `METIS_ENABLE_TLS=ON` for the post-quantum TLS
  layer. It is linked from the system, not vendored in this repository.
- The post-quantum hybrid group `X25519MLKEM768` (ML-KEM) requires OpenSSL
  3.5 or newer; older versions fall back to classical key exchange.
- License: Apache License 2.0.
- Project: https://www.openssl.org
