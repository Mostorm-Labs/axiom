@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 >out/p32-web-ui/native-host-env.log 2>&1
if errorlevel 1 exit /b %errorlevel%
cmake -S . -B out/p32-touch/build >out/p32-web-ui/native-host-configure.log 2>&1
if errorlevel 1 exit /b %errorlevel%
cmake --build out/p32-touch/build --target axiom_minimal_runtime_host_windows -j 4 >out/p32-web-ui/native-host-build.log 2>&1
exit /b %errorlevel%
