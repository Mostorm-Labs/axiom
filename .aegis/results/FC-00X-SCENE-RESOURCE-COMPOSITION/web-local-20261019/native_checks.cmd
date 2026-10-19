@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 >nul
if errorlevel 1 exit /b %errorlevel%
cmake --build out/p32-touch/build --target axiom_demo_web_surface_tests -j 4 >out/p32-web/native-web-surface-build.log 2>&1
if errorlevel 1 exit /b %errorlevel%
ctest --test-dir out/p32-touch/build -R "^G3WebSurfaceHost$" --output-on-failure --output-junit ../web-surface-tests.xml >out/p32-web/native-web-surface-tests.log 2>&1
exit /b %errorlevel%
