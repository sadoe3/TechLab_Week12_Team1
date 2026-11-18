param(
    [Parameter(Mandatory)][string]$HostExe,
    [Parameter(Mandatory)][string]$WriterExe,
    [Parameter(Mandatory)][string]$StubExe
)

$ErrorActionPreference = 'Stop'
$testRoot = Join-Path $env:TEMP ('CrashFailures-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory $testRoot | Out-Null

# 각 case의 sibling EXE만 사용한다. 실행 중인 Editor나 production writer를 변경하지 않는다.
foreach ($case in @('missing', 'startup-exit', 'writer-exit', 'timeout', 'write-failure')) {
    $caseDirectory = Join-Path $testRoot $case
    New-Item -ItemType Directory $caseDirectory | Out-Null
    $testHost = Join-Path $caseDirectory 'CrashProcessSmoke.exe'
    Copy-Item -LiteralPath $HostExe -Destination $testHost
    if ($case -eq 'write-failure') {
        Copy-Item -LiteralPath $WriterExe -Destination (Join-Path $caseDirectory 'CrashDumpWriter.exe')
    } elseif ($case -ne 'missing') {
        Copy-Item -LiteralPath $StubExe -Destination (Join-Path $caseDirectory 'CrashDumpWriter.exe')
    }

    $reportDirectory = Join-Path $caseDirectory 'Report'
    if ($case -eq 'write-failure') {
        $blockedPath = Join-Path $caseDirectory 'Blocked'
        Set-Content -LiteralPath $blockedPath -Value 'This is a file, not a directory.'
        $reportDirectory = Join-Path $blockedPath 'Report'
    }

    $start = New-Object System.Diagnostics.ProcessStartInfo
    $start.FileName = $testHost
    $start.Arguments = '"' + $reportDirectory + '" crash'
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.EnvironmentVariables['CRASH_TEST_WRITER_BEHAVIOR'] = $case
    $elapsed = [Diagnostics.Stopwatch]::StartNew()
    $testProcess = [Diagnostics.Process]::Start($start)
    try {
        if (!$testProcess.WaitForExit(25000)) {
            $testProcess.Kill()
            throw "$case exceeded 25 seconds"
        }
        $expectedExit = if ($case -eq 'missing') { 2 } elseif ($case -eq 'startup-exit') { 1067 } else { -1073741819 }
        if ($testProcess.ExitCode -ne $expectedExit) {
            throw "$case exit $($testProcess.ExitCode), expected $expectedExit"
        }
        if ($case -eq 'timeout' -and $elapsed.Elapsed.TotalSeconds -lt 14) {
            throw 'Timeout path returned before the configured wait'
        }
        if (Test-Path -LiteralPath (Join-Path $reportDirectory 'Crash.dmp')) {
            throw "$case unexpectedly produced a Dump"
        }
        [pscustomobject]@{ Case = $case; ExitCode = $testProcess.ExitCode; Seconds = [math]::Round($elapsed.Elapsed.TotalSeconds, 2) }
    } finally {
        $testProcess.Dispose()
    }
}

Write-Output "Evidence: $testRoot"
