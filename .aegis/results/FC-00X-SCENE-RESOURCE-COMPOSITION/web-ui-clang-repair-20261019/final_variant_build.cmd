@echo off
call .deps\emsdk\emsdk_env.bat >out/p32-web-ui/final-emsdk-env.log 2>&1
if errorlevel 1 exit /b %errorlevel%
python out/p32-web-ui/final_variant_build.py %1
exit /b %errorlevel%
