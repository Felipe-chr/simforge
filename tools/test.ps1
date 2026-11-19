#Requires -Version 5.1
<#
  Compila e executa os testes.

  Usa o preset release por padrao porque maquinas com WDAC/Smart App Control
  podem bloquear o executavel Debug (maior). Uso:

    .\tools\test.ps1
    .\tools\test.ps1 -Preset dev
    .\tools\test.ps1 -Filter engine_
#>
param(
    [ValidateSet('dev', 'release')]
    [string]$Preset = 'release',
    [string]$Filter = ''
)

$ErrorActionPreference = 'Stop'
$sourceDir = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$build = Join-Path $PSScriptRoot 'build.ps1'

& powershell -NoProfile -ExecutionPolicy Bypass -File $build -Preset $Preset
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$exe = Join-Path $sourceDir "build\$Preset\tests\simforge_tests.exe"
if (-not (Test-Path $exe)) {
    Write-Host "Executavel de teste nao encontrado: $exe" -ForegroundColor Red
    exit 1
}

$args = @()
if ($Filter) { $args += $Filter }
& $exe @args
exit $LASTEXITCODE
