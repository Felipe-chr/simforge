@echo off
rem Atalho para tools\run.ps1 sem depender da ExecutionPolicy.
rem Uso: tools\run.cmd [-Rebuild] [-Port 5174]
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0run.ps1" %*
exit /b %ERRORLEVEL%
