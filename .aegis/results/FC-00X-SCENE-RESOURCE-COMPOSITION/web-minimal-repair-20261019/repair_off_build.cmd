@echo off
call .deps\emsdk\emsdk_env.bat >out/p32-web/repair-off-emsdk-env.log 2>&1
if errorlevel 1 exit /b %errorlevel%
cmake -S . -B out/p32-web/build -DAXIOM_BUILD_DEBUG_UI=OFF -DAXIOM_DEBUG_UI_PROFILE=off >out/p32-web/repair-off-configure.log 2>&1
if errorlevel 1 exit /b %errorlevel%
cmake --build out/p32-web/build --target axiom_canvas_demo_web -j 4 >out/p32-web/repair-off-build.log 2>&1
exit /b %errorlevel%
