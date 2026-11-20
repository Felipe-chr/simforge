# Sobe a interface visual do SimForge em um comando.
#
#   tools\run.cmd                  # compila o núcleo se preciso e abre a UI
#   tools\run.cmd -Rebuild         # força recompilar o núcleo antes
#   tools\run.cmd -Port 5174       # outra porta
#   tools\run.cmd -NoOpen          # não abre o navegador
#
# Sem o executável do núcleo a UI ainda sobe, no modo mock.

[CmdletBinding()]
param(
    [switch]$Rebuild,
    [switch]$NoOpen,
    [int]$Port = 5173
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$web = Join-Path $root 'web'
$cli = Join-Path $root 'build\release\src\simforge-cli.exe'

function Find-Npm {
    $local = Join-Path $env:LOCALAPPDATA 'nodejs\npm.cmd'
    if (Test-Path $local) { return $local }
    $cmd = Get-Command npm.cmd -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    throw 'npm.cmd não encontrado. Instale o Node (https://nodejs.org) e tente de novo.'
}

$npm = Find-Npm
Write-Host "npm:    $npm"

# --- núcleo ---------------------------------------------------------------
if ($Rebuild -or -not (Test-Path $cli)) {
    Write-Host 'núcleo: compilando (preset release)...'
    & (Join-Path $PSScriptRoot 'build.ps1') -Preset release
    if (-not (Test-Path $cli)) { throw 'falha ao compilar o núcleo (veja a saída acima)' }
}

if (Test-Path $cli) {
    Write-Host "núcleo: $cli"
    Write-Host '        a UI fala com ele pela ponte do dev server'
} else {
    Write-Warning 'núcleo ausente: a UI vai subir no modo mock'
}

# --- dependências do frontend --------------------------------------------
if (-not (Test-Path (Join-Path $web 'node_modules'))) {
    Write-Host 'web:    instalando dependências (primeira vez)...'
    Push-Location $web
    try { & $npm install } finally { Pop-Location }
}

# --- dev server ----------------------------------------------------------
Write-Host "web:    http://localhost:$Port"
Write-Host '        Ctrl+C encerra'
$arguments = @('run', 'dev', '--', '--port', "$Port")
if (-not $NoOpen) { $arguments += '--open' }

Push-Location $web
try {
    & $npm @arguments
} finally {
    Pop-Location
}
