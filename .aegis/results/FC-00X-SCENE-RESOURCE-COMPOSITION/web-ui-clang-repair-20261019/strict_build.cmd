@echo off
call .deps\emsdk\emsdk_env.bat >out/p32-web-ui/emsdk-env.log 2>&1
if errorlevel 1 exit /b %errorlevel%
cmake -S . -B out/p32-web/build -DCMAKE_CXX_FLAGS=-fexceptions -DCMAKE_PROJECT_INCLUDE= -DAXIOM_BUILD_DEBUG_UI=ON -DAXIOM_DEBUG_UI_PROFILE=full >out/p32-web-ui/strict-configure.log 2>&1
if errorlevel 1 exit /b %errorlevel%
cmake --build out/p32-web/build --target axiom_canvas_demo_web -j 4 >out/p32-web-ui/strict-build.log 2>&1
exit /b %errorlevel%
