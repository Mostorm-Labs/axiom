$ErrorActionPreference = 'Stop'
$raw = '.aegis/results/AXIOM-SDK-01-ABI-CANDIDATE-006/raw'
$vs = 'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat'
$tool = 'C:\Users\Chado\AppData\Local\Axiom\toolchains\emsdk-6.0.6'
$env:EMSDK_PYTHON = 'C:\Users\Chado\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
$env:EMSDK_NODE = Join-Path $tool 'node\24.19.0_64bit\node.exe'
if (!(Test-Path -LiteralPath $env:EMSDK_NODE)) { throw 'Locked toolchain Node is missing' }
$env:PATH = (Join-Path $tool 'upstream\emscripten') + ';' + $env:PATH
$records = [System.Collections.Generic.List[object]]::new()
$native = 'call "' + $vs + '" >nul && cl /nologo /W4 /WX /std:c11 /I docs/api /I docs/planning/sdk /c verification/sdk01/multi_candidate_compile.c /Foout/sdk01-review/multi-candidate-c.obj && cl /nologo /W4 /WX /std:c11 /I docs/api /I docs/planning/sdk /c verification/sdk01/multi_candidate_consumer.c /Foout/sdk01-review/multi-candidate-consumer.obj && cl /nologo /W4 /WX /std:c11 /I docs/api /I docs/planning/sdk /c verification/sdk01/multi_candidate_leaf_consumer.c /Foout/sdk01-review/multi-candidate-leaf.obj && cl /nologo /W4 /WX /EHsc /std:c++17 /I docs/api /I docs/planning/sdk /c verification/sdk01/multi_candidate_compile.cpp /Foout/sdk01-review/multi-candidate-cpp.obj && cl /nologo /W4 /WX /std:c11 /I docs/api /I docs/planning/sdk verification/sdk01/multi_candidate_layout.c /Foout/sdk01-review/layout-native.obj /Feout/sdk01-review/layout-native.exe'
$nativeOutput = & cmd.exe /c $native 2>&1
$code = $LASTEXITCODE
$records.Add(@{name='native_compile'; command=$native; exit_code=$code; output=($nativeOutput | Out-String)})
if ($code -ne 0) { throw "Native compile failed: $code" }
$inc = @('-I','docs/api','-I','docs/planning/sdk')
$cases = @(
  @{name='wasm_c'; executable='emcc.bat'; args=@('-std=c11','-Wall','-Wextra','-Werror')+$inc+@('-c','verification/sdk01/multi_candidate_compile.c','-o','out/sdk01-review/multi-candidate-wasm.o')},
  @{name='wasm_consumer'; executable='emcc.bat'; args=@('-std=c11','-Wall','-Wextra','-Werror')+$inc+@('-c','verification/sdk01/multi_candidate_consumer.c','-o','out/sdk01-review/multi-candidate-consumer-wasm.o')},
  @{name='wasm_leaf'; executable='emcc.bat'; args=@('-std=c11','-Wall','-Wextra','-Werror')+$inc+@('-c','verification/sdk01/multi_candidate_leaf_consumer.c','-o','out/sdk01-review/multi-candidate-leaf-wasm.o')},
  @{name='wasm_cpp'; executable='em++.bat'; args=@('-std=c++17','-Wall','-Wextra','-Werror')+$inc+@('-c','verification/sdk01/multi_candidate_compile.cpp','-o','out/sdk01-review/multi-candidate-wasm-cpp.o')},
  @{name='wasm_layout_build'; executable='emcc.bat'; args=@('-std=c11','-Wall','-Wextra','-Werror')+$inc+@('verification/sdk01/multi_candidate_layout.c','-sENVIRONMENT=node','-o','out/sdk01-review/layout-wasm.js')}
)
foreach ($item in $cases) {
  $argsList = $item.args
  $output = & $item.executable @argsList 2>&1
  $code = $LASTEXITCODE
  $records.Add(@{name=$item.name; executable=$item.executable; args=$argsList; exit_code=$code; output=($output | Out-String)})
  if ($code -ne 0) { throw "$($item.name) failed: $code" }
}
$nativeLayout = & out/sdk01-review/layout-native.exe
if ($LASTEXITCODE -ne 0) { throw 'native layout failed' }
$wasmLayout = & $env:EMSDK_NODE out/sdk01-review/layout-wasm.js
if ($LASTEXITCODE -ne 0) { throw 'wasm layout failed' }
[System.IO.File]::WriteAllText((Join-Path (Resolve-Path $raw) 'native_layout.json'), ($nativeLayout + "`n"), [System.Text.UTF8Encoding]::new($false))
[System.IO.File]::WriteAllText((Join-Path (Resolve-Path $raw) 'wasm_layout.json'), ($wasmLayout + "`n"), [System.Text.UTF8Encoding]::new($false))
$compare = & python verification/sdk01/verify_multi_candidate_layout.py --native "$raw/native_layout.json" --wasm "$raw/wasm_layout.json" 2>&1
$code = $LASTEXITCODE
$records.Add(@{name='layout_compare'; command='python verification/sdk01/verify_multi_candidate_layout.py --native <raw/native_layout.json> --wasm <raw/wasm_layout.json>'; exit_code=$code; output=($compare | Out-String)})
$records | ConvertTo-Json -Depth 8 | Set-Content -Encoding utf8 "$raw/final-commands.json"
if ($code -ne 0) { throw "layout comparison failed: $code" }
$records | ForEach-Object { Write-Output "$($_.name) exit=$($_.exit_code)" }
