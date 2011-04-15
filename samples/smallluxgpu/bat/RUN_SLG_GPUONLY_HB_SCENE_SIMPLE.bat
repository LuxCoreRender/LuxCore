@echo off
SLG.exe -D renderengine.type 3 -l 0 -D screen.refresh.interval 1000 -w 1024 -e 768 -D opencl.cpu.use 1 scenes\simple\render-fast.cfg
pause
