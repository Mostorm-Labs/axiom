@echo off
call "C:\Users\Chado\AppData\Local\Axiom\toolchains\emsdk-6.0.6\emsdk_env.bat" >nul
if errorlevel 1 exit /b %errorlevel%
cmake --build out/d07-functional/web-build --target axiom_fc04_d07_web_intent axiom_canvas_demo_web --parallel 8
exit /b %errorlevel%
