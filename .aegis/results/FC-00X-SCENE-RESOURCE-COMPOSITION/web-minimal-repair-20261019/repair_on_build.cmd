@echo off
call .deps\emsdk\emsdk_env.bat >out/p32-web/repair-on-emsdk-env.log 2>&1
if errorlevel 1 exit /b %errorlevel%
cmake -S . -B out/p32-web/build -DAXIOM_BUILD_DEBUG_UI=ON -DAXIOM_DEBUG_UI_PROFILE=full >out/p32-web/repair-on-configure.log 2>&1
if errorlevel 1 exit /b %errorlevel%
cmake --build out/p32-web/build --target axiom_canvas_demo_web -j 4 >out/p32-web/repair-on-build.log 2>&1
exit /b %errorlevel%
