@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 >out/p32-web-ui/vs-env.log 2>&1
if errorlevel 1 exit /b %errorlevel%
cl >out/p32-web-ui/msvc-version.log 2>&1
python out/p32-web-ui/native_checks.py
exit /b %errorlevel%
