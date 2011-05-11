@echo off
SLG2.exe -D renderengine.type 4 -D screen.refresh.interval 50 -D path.sampler.type INLINED_RANDOM scenes\simple\render-fast.cfg
pause
