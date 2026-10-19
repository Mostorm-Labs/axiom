param(
    [string]$BuildDirectory = 'out/fc01-closeout/build',
    [string]$VcVars = 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat'
)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '../../../..')).Path
$library = (Resolve-Path (Join-Path $repo "$BuildDirectory/runtime/semantic/canvas_runtime_semantic.lib")).Path
$output = Join-Path $repo 'out/fc02-semantic-contract'
New-Item -ItemType Directory -Force -Path $output | Out-Null
$source = Join-Path $PSScriptRoot 'rich_text_inverse_probe.cpp'
$exe = Join-Path $output 'rich_text_inverse_probe.exe'
$obj = Join-Path $output 'rich_text_inverse_probe.obj'
$commands = @"
@echo off
call "$VcVars"
if errorlevel 1 exit /b 2
cl /nologo /std:c++20 /EHsc /W4 /WX /MT /O2 /I"$repo/runtime/semantic/include" /I"$repo/runtime/semantic/src" /I"$repo/runtime/foundation/include" "$source" "$library" /Fo"$obj" /Fe"$exe"
"@
$batch = Join-Path $output 'compile-probe.cmd'
Set-Content -LiteralPath $batch -Value $commands -Encoding utf8
& $env:ComSpec /d /c $batch 2>&1 | Tee-Object -FilePath (Join-Path $output 'compile.log')
if ($LASTEXITCODE -ne 0) { throw "Probe compile failed: $LASTEXITCODE" }
& $exe 2>&1 | Tee-Object -FilePath (Join-Path $output 'inverse-probe.log')
if ($LASTEXITCODE -ne 0) { throw "Probe precondition/counterexample failed: $LASTEXITCODE" }
git -C $repo rev-parse HEAD
Get-FileHash -Algorithm SHA256 -LiteralPath $exe
