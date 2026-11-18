<#
용도: C5에서 준비한 EXE/PDB를 Microsoft Symbol Store에 복사 게시합니다.
Build.json은 이 script의 게시 입력 목록이며 debugger가 읽는 파일은 아닙니다.
원본 build를 다시 실행하지 않습니다. 실패 시 같은 staging 경로로 재시도할 수 있습니다.
#>
#requires -Version 5.1
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$StagingDirectory,
    [string]$Store = 'C:\SymbolStore',
    [string]$DebugTools = (Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\Debuggers\x64')
)
$ErrorActionPreference = 'Stop'
$publishLock = $null
try {
    # 1. 준비 완료 기록과 파일 hash를 확인하고, 게시할 파일만 목록에 넣습니다.
    $stagingPath = [IO.Path]::GetFullPath($StagingDirectory)
    $manifest = Get-Content -LiteralPath (Join-Path $stagingPath 'Build.json') -Raw | ConvertFrom-Json
    if ($manifest.Status -ne 'IndexedAndVerified' -or @($manifest.Modules).Count -eq 0) {
        throw 'The staging manifest is not ready for publishing.'
    }
    $symbolFiles = @()
    foreach ($module in $manifest.Modules) {
        if ($module.Module -notin @('HitoriEditor', 'HitoriGame', 'CrashDumpWriter')) { throw 'Unsupported module in manifest.' }
        foreach ($extension in @('exe', 'pdb')) {
            $symbolPath = Join-Path $stagingPath ($module.Module + '.' + $extension)
            $expectedHash = if ($extension -eq 'exe') { $module.ExeSHA256 } else { $module.PdbSHA256 }
            if ((Get-FileHash -LiteralPath $symbolPath -Algorithm SHA256).Hash -ne $expectedHash) { throw "Staged file changed: $symbolPath" }
            $symbolFiles += $symbolPath
        }
    }
    $symstore = Join-Path $DebugTools 'symstore.exe'
    if (!(Test-Path -LiteralPath $symstore)) { throw 'symstore.exe is missing. Run SetupCrashTools.bat.' }

    # 2. SymStore는 동시 게시를 지원하지 않습니다. 이 script끼리는 store의 파일 lock으로 동시 게시를 거부합니다.
    $storePath = [IO.Path]::GetFullPath($Store)
    [IO.Directory]::CreateDirectory($storePath) | Out-Null
    $publishLock = [IO.File]::Open((Join-Path $storePath '.publish.lock'), 'OpenOrCreate', 'ReadWrite', 'None')
    $fileList = Join-Path $stagingPath 'PublishFiles.txt'
    [IO.File]::WriteAllLines($fileList, $symbolFiles, [Text.Encoding]::Default)

    # 3. /p를 사용하지 않아 staging을 가리키는 pointer 대신 실제 파일을 보관합니다.
    $logPath = Join-Path $stagingPath 'symstore.log'
    & $symstore add /f "@$fileList" /s $storePath /t Hitori /v $manifest.BuildId /c $manifest.Commit /d $logPath
    if ($LASTEXITCODE -ne 0) { throw "Symbol publishing failed. See $logPath; retry with the same staging directory." }
    Write-Output "Published symbols: $storePath (log: $logPath)"
}
catch {
    [Console]::Error.WriteLine("Symbol publish failed: $($_.Exception.Message)")
    exit 1
}
finally {
    if ($null -ne $publishLock) { $publishLock.Dispose() }
}
