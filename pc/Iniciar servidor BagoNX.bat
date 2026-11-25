@echo off
title BagoNX PC - juegos por red local
cd /d "%~dp0"
where py >nul 2>nul && (py -3 bagonx_pc.py "%~dp0..\juegos") || (python bagonx_pc.py "%~dp0..\juegos")
pause
