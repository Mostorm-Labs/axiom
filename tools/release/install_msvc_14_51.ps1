$ErrorActionPreference = 'Stop'

# Select the same Visual Studio instance as the following msvc-dev-cmd step.
$installerDirectory = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer'
$vswhere = Join-Path $installerDirectory 'vswhere.exe'
$installation = & $vswhere -latest -products '*' -prerelease -property installationPath
if ($LASTEXITCODE -ne 0 -or -not $installation) {
    throw 'Could not locate the hosted Visual Studio installation'
}

$compiler = Join-Path $installation 'VC\Tools\MSVC\14.51.*\bin\Hostx64\x64\cl.exe'
if (-not (Test-Path -Path $compiler -PathType Leaf)) {
    $arguments = @(
        'modify', '--installPath', ('"{0}"' -f $installation),
        '--add', 'Microsoft.VisualStudio.Component.VC.14.51.x86.x64',
        '--quiet', '--norestart'
    )
    $result = Start-Process -FilePath (Join-Path $installerDirectory 'setup.exe') `
        -ArgumentList $arguments -PassThru -Wait
    if ($result.ExitCode -notin 0, 3010) {
        throw "Visual Studio Installer failed to add MSVC 14.51 (exit $($result.ExitCode))"
    }
}

if (-not (Test-Path -Path $compiler -PathType Leaf)) {
    throw 'MSVC 14.51 x64 is required to build against the locked Windows SDKs'
}
Get-Item -Path $compiler | ForEach-Object { Write-Host $_.FullName }
