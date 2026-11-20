@echo off
rem Atalho para tools\build.ps1 sem depender da ExecutionPolicy.
rem Uso: tools\build.cmd [-Preset release] [-Test] [-Clean]
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build.ps1" %*
exit /b %ERRORLEVEL%
