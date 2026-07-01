$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$vcvars = 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat'
$buildLog = Join-Path $root 'build.log'
$objDir = Join-Path $root 'out'
$exe = Join-Path $root 'yarnspinner_harness.exe'
$runLog = Join-Path $root 'run.log'
$runErrLog = Join-Path $root 'run.err.log'
$includeDir = Join-Path $root 'include'
$vendorDir = Join-Path $root 'vendor'
$generatedDir = Join-Path $vendorDir 'generated'
$protobufIncludeDir = Join-Path $root 'third_party\YSProtobuf\include'
$protobufLibDir = Join-Path $root 'third_party\YSProtobuf\lib'

function Invoke-LoggedCommand {
    param(
        [string]$Label,
        [string]$Command,
        [int]$TimeoutSeconds = 180
    )

    Add-Content -Path $buildLog -Value ">>> $Label"
    Write-Host ">>> $Label"

    $output = & cmd /c $Command 2>&1
    $exitCode = $LASTEXITCODE

    $output | Tee-Object -FilePath $buildLog -Append | Out-Host

    if ($exitCode -ne 0) {
        throw "$Label failed with exit code $exitCode"
    }
}

if (-not (Test-Path $vcvars)) {
    throw "vcvars64.bat was not found at $vcvars"
}

New-Item -ItemType Directory -Path $objDir -Force | Out-Null
Set-Content -Path $buildLog -Value ''

$targets = @(
    @{ Source = Join-Path $root 'src\main.cpp'; Object = Join-Path $objDir 'main.obj' },
    @{ Source = Join-Path $generatedDir 'yarn_spinner.pb.cc'; Object = Join-Path $objDir 'yarn_spinner.pb.obj' }
)

foreach ($target in $targets) {
    $command = "call `"$vcvars`" && cd /d `"$root`" && cl /nologo /std:c++20 /EHsc /MD /DYARNSPINNER_API= /I`"$includeDir`" /I`"$generatedDir`" /I`"$vendorDir`" /I`"$protobufIncludeDir`" /c `"$($target.Source)`" /Fo`"$($target.Object)`""
    Invoke-LoggedCommand -Label "Compiling $($target.Source)" -Command $command
}

$linkInputs = ($targets | ForEach-Object { '"' + $_.Object + '"' }) -join ' '
$command = "call `"$vcvars`" && cd /d `"$root`" && link /nologo /OUT:`"$exe`" $linkInputs /LIBPATH:`"$protobufLibDir`" libprotobuf.lib"
Invoke-LoggedCommand -Label 'Linking harness' -Command $command

if (-not (Test-Path $exe)) {
    throw "Expected executable was not produced at $exe"
}

Add-Content -Path $buildLog -Value '>>> Running harness smoke test'
Write-Host '>>> Running harness smoke test'
$ysaPath = Join-Path $root 'assets\sample_project\out\main.ysa'
try {
    $process = Start-Process -FilePath $exe -ArgumentList @($ysaPath, '--start', 'Ending') -PassThru -NoNewWindow -RedirectStandardOutput $runLog -RedirectStandardError $runErrLog
    if (-not $process.WaitForExit(20000)) {
        Stop-Process -Id $process.Id -Force
        throw 'Harness did not exit within 20 seconds'
    }
    $exitCode = $process.ExitCode
    if ($null -ne $exitCode -and $exitCode -ne 0) {
        throw "Harness exited with code $($exitCode)"
    }
} catch {
    Add-Content -Path $buildLog -Value $_.Exception.Message
    throw
}

Write-Host 'Harness completed successfully'
