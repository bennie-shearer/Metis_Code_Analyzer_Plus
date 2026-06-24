# CI/CD Integration -- Metis Code Analyzer Plus

Metis Code Analyzer Plus runs as a self-contained binary and exposes a REST
API. The two primary integration patterns for CI/CD pipelines are:

1. **Direct invocation**: run the binary inside the pipeline job, use the
   `gate.exit_nonzero` option to fail the build on quality regression.
2. **Persistent server**: a long-running Metis instance is already deployed;
   the pipeline POSTs to `/api/scan` and GETs `/api/gate` to check the result.

---

## Configuration for CI/CD

Enable the quality gate in `code_analyzer.pson`:

```
gate.enabled        = true
gate.min_tqi        = 80.0
gate.max_critical   = 0
gate.exit_nonzero   = true
```

Enable API key authentication for headless callers:

```
auth.api_key_enabled = true
auth.api_key_sha256  = "<SHA-256 of your key>"
```

Generate the hash via: `POST /api/hash-password with body {"value":"<password>"} (or GET /api/hash-password?value=<text> -- note: encode # as %23 in GET URLs)` (unauthenticated).

---

## GitHub Actions

```yaml
# .github/workflows/code-quality.yml
name: Code Quality

on: [push, pull_request]

jobs:
  analyze:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Download Metis Code Analyzer Plus
        run: |
          curl -L "https://your-download-url/metis_code_analyzer_plus" \
            -o metis && chmod +x metis

      - name: Run quality scan
        run: |
          cat > ci.pson << 'EOF'
          server.port = 8080
          scan.root = "."
          scan.on_start = true
          gate.enabled = true
          gate.min_tqi = 80.0
          gate.max_critical = 0
          gate.exit_nonzero = true
          auth.enabled = false
          EOF
          ./metis ci.pson &
          sleep 5
          GATE=$(curl -sf http://localhost:8080/api/gate)
          echo "$GATE"
          echo "$GATE" | grep -q '"passed":true' || exit 1

      - name: Upload report
        if: always()
        run: |
          curl -sf http://localhost:8080/api/report.json \
            -o metis-report.json || true

      - uses: actions/upload-artifact@v4
        if: always()
        with:
          name: metis-report
          path: metis-report.json
```

---

## Jenkins Pipeline (Declarative)

```groovy
// Jenkinsfile
pipeline {
    agent any

    stages {
        stage('Code Quality') {
            steps {
                script {
                    sh '''
                        # Start Metis
                        ./metis code_analyzer.pson &
                        sleep 5

                        # Check gate
                        GATE=$(curl -sf -H "X-API-Key: $METIS_API_KEY" \
                            http://localhost:8080/api/gate)
                        echo "$GATE"
                        echo "$GATE" | python3 -c "
import sys, json
d = json.load(sys.stdin)
print('TQI:', d['tqi'], 'Gate:', 'PASS' if d['passed'] else 'FAIL')
sys.exit(0 if d['passed'] else 1)
"
                    '''
                }
            }
            post {
                always {
                    sh '''
                        curl -sf -H "X-API-Key: $METIS_API_KEY" \
                            http://localhost:8080/api/report.json \
                            -o metis-report.json || true
                    '''
                    archiveArtifacts artifacts: 'metis-report.json',
                                     allowEmptyArchive: true
                }
            }
        }
    }
}
```

---

## GitLab CI

```yaml
# .gitlab-ci.yml
code-quality:
  stage: test
  image: ubuntu:24.04
  before_script:
    - apt-get update -qq && apt-get install -y -qq curl
    - curl -L "$METIS_BINARY_URL" -o metis && chmod +x metis
  script:
    - |
      cat > ci.pson << 'EOF'
      server.port = 8080
      scan.root = "."
      scan.on_start = true
      gate.enabled = true
      gate.min_tqi = 75.0
      gate.max_critical = 0
      auth.enabled = false
      EOF
      ./metis ci.pson &
      sleep 5
      curl -sf http://localhost:8080/api/gate | tee gate.json
      grep -q '"passed":true' gate.json
  artifacts:
    when: always
    paths:
      - gate.json
    reports:
      # Optionally convert to Code Quality format for MR decoration
      codequality: gate.json
```

---

## Persistent Server: POST /api/scan

If you run Metis as a persistent service, pipelines can trigger a rescan
and poll for results:

```sh
# Trigger scan
curl -X POST http://metis.internal:8080/api/scan \
  -H "X-API-Key: $API_KEY" \
  -H "Content-Type: application/json" \
  -d '{"root":"/builds/myproject"}'

# Check gate
curl -sf http://metis.internal:8080/api/gate \
  -H "X-API-Key: $API_KEY"
```

Response from `/api/gate`:

```json
{
  "enabled": true,
  "passed": true,
  "reason": "",
  "tqi": 94.3,
  "issues": 2,
  "critical": 0,
  "min_tqi": 80.0,
  "max_issues": -1,
  "max_critical": 0
}
```

Exit code: pipeline scripts can check `"passed": true` or rely on
`gate.exit_nonzero = true` when running Metis inline.
