@echo off
call .deps\emsdk\emsdk_env.bat >out/p32-web/emsdk-env.log 2>&1
if errorlevel 1 exit /b %errorlevel%
emcc --version >out/p32-web/toolchain.txt 2>&1
python out/p32-web/configure.py %* >out/p32-web/configure.log 2>&1
if errorlevel 1 exit /b %errorlevel%
cmake --build out/p32-web/build --target axiom_ink_playground_web axiom_canvas_demo_web -j 4 >out/p32-web/build.log 2>&1
exit /b %errorlevel%
