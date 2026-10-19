@echo off
call .deps\emsdk\emsdk_env.bat >out/fc01-closeout/emsdk-env.log 2>&1
if errorlevel 1 exit /b %errorlevel%
python out/fc01-closeout/final_web.py %1
exit /b %errorlevel%
