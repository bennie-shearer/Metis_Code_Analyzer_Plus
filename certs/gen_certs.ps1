# Metis Code Analyzer Plus - Generate TLS certificate for localhost
# Run this script ONCE as Administrator. It:
#   1. Creates a self-signed certificate in the machine Trusted Root store
#      so Edge and Chrome trust it automatically (green padlock).
#   2. Exports server.crt and server.key to this certs\ folder (source tree).
#   3. CMake's POST_BUILD step copies certs\ to cmake-build-release\certs\
#      automatically on the next build -- no manual file copying needed.
#
# Usage (Administrator PowerShell from the PROJECT ROOT):
#   powershell -ExecutionPolicy Bypass -File certs\gen_certs.ps1
#
# After running: do a Rebuild in CLion, then restart the server.

param(
    [int]$ValidYears = 10
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$CertFile  = Join-Path $ScriptDir "server.crt"
$KeyFile   = Join-Path $ScriptDir "server.key"
$PfxFile   = Join-Path $ScriptDir "temp.pfx"
$PfxPwd    = "MetisTempPfx2026"

Write-Host "Metis Code Analyzer Plus - TLS certificate generator" -ForegroundColor Cyan
Write-Host "------------------------------------------------------"

# Check admin
$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
    [Security.Principal.WindowsBuiltinRole]::Administrator)
if (-not $isAdmin) {
    Write-Host "ERROR: Run this script as Administrator (right-click PowerShell -> Run as administrator)." -ForegroundColor Red
    exit 1
}

# Remove old cert from Root store if present
Get-ChildItem "Cert:\LocalMachine\Root" | Where-Object {
    $_.Subject -like "*Metis Code Analyzer Plus*"
} | ForEach-Object {
    Write-Host "  Removing old cert: $($_.Thumbprint)"
    Remove-Item "Cert:\LocalMachine\Root\$($_.Thumbprint)" -Force
}

# Generate new cert directly in the machine Trusted Root store.
# Edge and Chrome read Cert:\LocalMachine\Root - being here IS the green padlock.
Write-Host "  Generating RSA-2048 certificate valid for $ValidYears years..."
$params = @{
    Subject             = "CN=Metis Code Analyzer Plus"
    DnsName             = @("localhost")
    CertStoreLocation   = "Cert:\LocalMachine\Root"
    NotAfter            = (Get-Date).AddYears($ValidYears)
    HashAlgorithm       = "SHA256"
    KeyAlgorithm        = "RSA"
    KeyLength           = 2048
    KeyExportPolicy     = "Exportable"
    KeyUsage            = @("DigitalSignature","KeyEncipherment")
    TextExtension       = @(
        "2.5.29.17={text}DNS=localhost&IPAddress=127.0.0.1&IPAddress=::1",
        "2.5.29.37={text}1.3.6.1.5.5.7.3.1"
    )
}
$cert = New-SelfSignedCertificate @params
Write-Host "  Thumbprint: $($cert.Thumbprint)" -ForegroundColor Green
Write-Host "  Expires:    $($cert.NotAfter)"

# Export PFX (contains both cert and private key)
$secPwd = ConvertTo-SecureString $PfxPwd -AsPlainText -Force
Export-PfxCertificate -Cert $cert -FilePath $PfxFile -Password $secPwd | Out-Null

# Export certificate PEM
$certBase64 = [Convert]::ToBase64String($cert.RawData, "InsertLineBreaks")
"-----BEGIN CERTIFICATE-----`n$certBase64`n-----END CERTIFICATE-----" |
    Set-Content -Path $CertFile -Encoding ASCII
Write-Host "  Wrote: $CertFile"

# Export private key PEM via .NET (no external tools required)
$pfxBytes  = [System.IO.File]::ReadAllBytes($PfxFile)
$pfxCert   = [System.Security.Cryptography.X509Certificates.X509Certificate2]::new(
                 $pfxBytes, $PfxPwd,
                 [System.Security.Cryptography.X509Certificates.X509KeyStorageFlags]::Exportable)
$rsa       = [System.Security.Cryptography.X509Certificates.RSACertificateExtensions]::GetRSAPrivateKey($pfxCert)
$keyBytes  = $rsa.ExportRSAPrivateKey()
$keyBase64 = [Convert]::ToBase64String($keyBytes, "InsertLineBreaks")
$keyHeader = "-----BEGIN RSA" + " PRIVATE KEY-----"
$keyFooter = "-----END RSA" + " PRIVATE KEY-----"
"$keyHeader`n$keyBase64`n$keyFooter" |
    Set-Content -Path $KeyFile -Encoding ASCII
Write-Host "  Wrote: $KeyFile"

# Clean up temp PFX
Remove-Item $PfxFile -Force

Write-Host ""
Write-Host "Done. Certificate installed in Cert:\LocalMachine\Root." -ForegroundColor Green
Write-Host "Restart Edge completely (taskkill /F /IM msedge.exe /T), then"
Write-Host "open https://localhost:8080 - you should see a green padlock."
