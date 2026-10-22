param([Parameter(Mandatory=$true)][string]$Stage)
$ErrorActionPreference='Stop'
$taskEvidenceDirectory=$PSScriptRoot
$taskDirectories=Get-Content -LiteralPath "$taskEvidenceDirectory\staged-directories.json" -Raw | ConvertFrom-Json
$taskDirectory=$taskDirectories.$Stage
if (!$taskDirectory) { throw 'Unknown stage' }
$taskExe=Join-Path $taskDirectory 'axiom_minimal_runtime_host_windows-3d7afa35.exe'
$env:AXIOM_MINIMAL_INPUT_LOG=Join-Path $taskEvidenceDirectory "$Stage-input.log"
$taskLaunch=[ordered]@{
 stage=$Stage;started_utc=(Get-Date).ToUniversalTime().ToString('o');requested_path=$taskExe;
 working_directory=$taskDirectory;sha256=(Get-FileHash -LiteralPath $taskExe).Hash.ToLowerInvariant();
 source_revision='3d7afa351dfbd42f18c449bda35f861089b1fb4f';input_log=$env:AXIOM_MINIMAL_INPUT_LOG;
 stdout=(Join-Path $taskEvidenceDirectory "$Stage-stdout.log");stderr=(Join-Path $taskEvidenceDirectory "$Stage-stderr.log")
}
try {
 $taskProcess=Start-Process -FilePath $taskExe -WorkingDirectory $taskDirectory -RedirectStandardOutput $taskLaunch.stdout -RedirectStandardError $taskLaunch.stderr -PassThru -ErrorAction Stop
 $taskLaunch.pid=$taskProcess.Id
 $taskRunning=Get-CimInstance Win32_Process -Filter "ProcessId=$($taskProcess.Id)" -ErrorAction SilentlyContinue
 $taskLaunch.actual_process_path=$taskRunning.ExecutablePath
 $taskLaunch.command_line=$taskRunning.CommandLine
 $taskExited=$taskProcess.WaitForExit(2500)
 $taskProcess.Refresh()
 $taskLaunch.has_exited=$taskProcess.HasExited
 $taskLaunch.exit_code=if($taskExited){$taskProcess.ExitCode}else{$null}
 $taskLaunch.main_window_handle=if(!$taskProcess.HasExited){$taskProcess.MainWindowHandle.ToInt64()}else{0}
 $taskLaunch.responding=if(!$taskProcess.HasExited){$taskProcess.Responding}else{$null}
 if (!$taskProcess.HasExited) {
  $taskProcess.Modules | Select-Object ModuleName,FileName,@{n='version';e={$_.FileVersionInfo.FileVersion}} | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath "$taskEvidenceDirectory\$Stage-loaded-modules.json" -Encoding utf8
 }
} catch { $taskLaunch.launch_exception=$_.Exception.ToString();$taskLaunch.error_record=$_.ToString() }
$taskLaunch | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath "$taskEvidenceDirectory\$Stage-launch.json" -Encoding utf8
$taskLaunch | ConvertTo-Json -Depth 5
Get-Content -LiteralPath $taskLaunch.stderr -ErrorAction SilentlyContinue
Get-Content -LiteralPath $env:AXIOM_MINIMAL_INPUT_LOG -ErrorAction SilentlyContinue -Head 8
