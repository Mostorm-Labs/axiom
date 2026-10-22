param(
    [string]$VcVars = 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat'
)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '../../../..')).Path
$output = Join-Path $repo 'out/fc02-input-contract'
New-Item -ItemType Directory -Force -Path $output | Out-Null
$source = Join-Path $PSScriptRoot 'fake_ime_contract_probe.cpp'
$exe = Join-Path $output 'fake_ime_contract_probe.exe'
$obj = Join-Path $output 'fake_ime_contract_probe.obj'
$batch = Join-Path $output 'compile-input-probe.cmd'
$commands = @"
@echo off
call "$VcVars"
if errorlevel 1 exit /b 2
cl /nologo /std:c++20 /EHsc /W4 /WX /MT /O2 /I"$repo/runtime/foundation/include" "$source" /Fo"$obj" /Fe"$exe"
"@
Set-Content -LiteralPath $batch -Value $commands -Encoding utf8
& $env:ComSpec /d /c $batch 2>&1 | Tee-Object -FilePath (Join-Path $output 'compile.log')
if ($LASTEXITCODE -ne 0) { throw "Input protocol probe compilation failed: $LASTEXITCODE" }
& $exe 2>&1 | Tee-Object -FilePath (Join-Path $output 'probe.log')
if ($LASTEXITCODE -ne 0) { throw "Input protocol model failed: $LASTEXITCODE" }
git -C $repo rev-parse HEAD
(Get-FileHash -Algorithm SHA256 -LiteralPath $exe).Hash
