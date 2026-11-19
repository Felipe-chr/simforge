#Requires -Version 5.1
<#
  Configura, compila e (opcionalmente) testa o SimForge no Windows.

  Uso:
    .\tools\build.ps1                 # Debug + build
    .\tools\build.ps1 -Test           # Debug + build + ctest
    .\tools\build.ps1 -Preset release -Test
    .\tools\build.ps1 -Target simforge_cli
#>
param(
    [ValidateSet('dev', 'release')]
    [string]$Preset = 'dev',
    [switch]$Test,
    [string]$Target = 'all'
)

$ErrorActionPreference = 'Stop'

$sourceDir = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Push-Location $sourceDir
try {

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $vswhere)) {
    throw "vswhere nao encontrado. Instale o Visual Studio com o workload C++."
}

$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw "Nenhum Visual Studio com ferramentas C++ encontrado." }

$cmake = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$ninjaDir = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja'
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
foreach ($p in @($cmake, $vcvars)) {
    if (-not (Test-Path $p)) { throw "Componente ausente: $p" }
}

$ninja = Join-Path $ninjaDir 'ninja.exe'
$steps = @(
    "`"$vcvars`" >nul",
    "`"$cmake`" --preset $Preset -DCMAKE_MAKE_PROGRAM=`"$ninja`"",
    "`"$cmake`" --build --preset $Preset --target $Target"
)
if ($Test) {
    $steps += "ctest --preset $Preset"
}

$command = $steps -join ' && '
cmd /c $command
if ($LASTEXITCODE -ne 0) {
    Write-Host "Falha no build (exit $LASTEXITCODE)" -ForegroundColor Red
    exit $LASTEXITCODE
}
Write-Host "OK - preset $Preset" -ForegroundColor Green
} finally {
    Pop-Location
}
