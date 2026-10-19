$ErrorActionPreference = 'Stop'
$runDir = (Resolve-Path 'out/fc01-closeout').Path
$runDir = Join-Path $runDir 'native-identity-startup'
New-Item -ItemType Directory -Path $runDir | Out-Null
$exePath = (Resolve-Path 'out/fc01-closeout/build/apps/minimal_runtime_host/axiom_minimal_runtime_host_windows.exe').Path
$revision = (git rev-parse HEAD).Trim()
$binaryHash = (Get-FileHash -LiteralPath $exePath -Algorithm SHA256).Hash.ToLowerInvariant()
$previousLog = $env:AXIOM_MINIMAL_INPUT_LOG
$env:AXIOM_MINIMAL_INPUT_LOG = Join-Path $runDir 'input.log'
try {
  $process = Start-Process -FilePath $exePath -WorkingDirectory $runDir -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $runDir 'native-stdout.log') -RedirectStandardError (Join-Path $runDir 'native-stderr.log')
} finally {
  $env:AXIOM_MINIMAL_INPUT_LOG = $previousLog
}
Start-Sleep -Milliseconds 2000
$process.Refresh()
$observed = Get-CimInstance Win32_Process -Filter "ProcessId=$($process.Id)" | Select-Object ProcessId,ExecutablePath
$facts = [ordered]@{
  source_revision = $revision
  exe_path = $exePath
  sha256 = $binaryHash
  pid = $process.Id
  process_observation = $observed
  running_after_startup = -not $process.HasExited
  exact_process_path_match = ($null -ne $observed -and $observed.ExecutablePath -eq $exePath)
  debug_ui_build = 'ON/full'
  input_action = 'None; CLI startup/lifecycle smoke only'
  physical_result = 'PHYSICAL_PENDING'
  new_manual_functional_claim = $false
}
if (-not $process.HasExited) {
  $facts.close_requested = $process.CloseMainWindow()
  if (-not $process.WaitForExit(3000)) {
    Stop-Process -Id $process.Id
    $facts.exit_method = 'Stopped the owned smoke process; no clean-shutdown claim'
  } else {
    $facts.exit_method = 'WM_CLOSE'
    $facts.exit_code = $process.ExitCode
  }
} else {
  $facts.exit_code = $process.ExitCode
}
$facts | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $runDir 'native-startup.json')
$facts | ConvertTo-Json -Depth 6
