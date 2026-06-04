# Triggers Aseprite build on your fork (miniherb13/aseprite-builder).
# Prerequisite: gh auth login  (one-time, in terminal)

$ErrorActionPreference = "Stop"
$repo = "miniherb13/aseprite-builder"
$workflow = "build_and_release.yaml"

if (-not (Get-Command gh -ErrorAction SilentlyContinue)) {
  Write-Host "GitHub CLI not found. Install: winget install GitHub.cli"
  exit 1
}

$auth = gh auth status 2>&1
if ($LASTEXITCODE -ne 0) {
  Write-Host "GitHub login required. Run this once, then run this script again:"
  Write-Host "  gh auth login"
  Write-Host ""
  Write-Host "Choose: GitHub.com -> HTTPS -> Login with browser"
  exit 1
}

Write-Host "Starting workflow: Build and release Aseprite ..."
gh workflow run $workflow --repo $repo --ref main

Write-Host ""
Write-Host "Watch build:"
Write-Host "  https://github.com/$repo/actions"
Write-Host ""
Write-Host "When finished, download Windows build from:"
Write-Host "  https://github.com/$repo/releases"
Write-Host ""
Write-Host "EULA: personal use only; delete public Release assets after download."
