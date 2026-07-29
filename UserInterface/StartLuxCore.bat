@echo off
REM StartLuxCore.bat - launches the LuxCore render controller from this
REM directory with the windowless pythonw interpreter; Python errors are
REM appended to render.log by the controller itself. Double-clicking a
REM .bat still flashes a console for an instant - use StartLuxCore.vbs
REM for a launch with no window at all.
cd /d "%~dp0"
start "" pythonw camera_controller.py
