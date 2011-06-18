@echo off
SLG.exe -D renderengine.type 4 -D screen.refresh.interval 100 -D path.sampler.type METROPOLIS scenes\luxball\render-fast.cfg
pause
