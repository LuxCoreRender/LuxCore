@echo off
REM StartLuxCore.bat - launches the LuxCore render controller from this
REM directory. It starts with the empty scene.scn stage and the default
REM hdre_055.hdr environment; geometry streams in through the TCP control
REM interface described in README.md.
cd /d "%~dp0"
start "LuxCore Render Controller" cmd /c "python camera_controller.py > render.log 2>&1"
