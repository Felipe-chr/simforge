@echo off
rem Atalho para tools\run.ps1 sem depender da ExecutionPolicy.
rem Uso: tools\run.cmd [-Rebuild] [-Port 5174] [-NoOpen]
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0run.ps1" %*
if errorlevel 1 (
  echo.
  echo Falhou. Pressione uma tecla para fechar.
  pause >nul
)
exit /b %ERRORLEVEL%
