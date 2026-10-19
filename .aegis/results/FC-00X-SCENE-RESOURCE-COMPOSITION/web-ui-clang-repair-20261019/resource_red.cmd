@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 >nul
if errorlevel 1 exit /b %errorlevel%
cmake --build out/p32-touch/build --target canvas_semantic_apply_plan_test -j 4 >out/p32-web-ui/resource-red-build.log 2>&1
if errorlevel 1 exit /b %errorlevel%
out\p32-touch\build\runtime\semantic\tests\canvas_semantic_apply_plan_test.exe --gtest_filter=ApplyPlan.ResourceRemovalChecksEverySupportedContentReference >out/p32-web-ui/resource-red.log 2>&1
exit /b %errorlevel%
