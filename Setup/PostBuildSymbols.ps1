<#
용도: VS post-build에서 선택적으로 staging과 symbol 게시를 연결합니다.
VS property sheet의 User Macro를 인자로 받아 게시 여부와 경로를 정합니다.
Publish=1일 때만 실행하며 기본값은 OFF입니다. 환경 변수·VS 재시작은 필요 없습니다.
#>
#requires -Version 5.1
[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release', 'Shipping')][string]$Configuration = 'Debug',
    [ValidateSet('HitoriEditor', 'HitoriGame')][string]$Target = 'HitoriEditor',
    [ValidateSet('0', '1')][string]$Publish = '0',
    [string]$Store = 'C:\SymbolStore'
)
$ErrorActionPreference = 'Stop'
if ($Publish -ne '1') {
    Write-Output 'Symbol publishing OFF (HitoriPublishSymbols is 0).'
    exit 0
}
try {
    # 별도 PowerShell에서 실행해 각 script의 exit code를 build 결과에 전달합니다.
    $stageOutput = @(& powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot/StageSymbols.ps1" -Configuration $Configuration -Target $Target)
    $stageExitCode = $LASTEXITCODE
    $stageOutput | Write-Output
    if ($stageExitCode -ne 0) { throw 'Symbol staging failed; publishing was not started.' }
    $readyPrefix = 'Ready for symbol publishing (not published): '
    $readyLine = $stageOutput | Where-Object { $_.StartsWith($readyPrefix) } | Select-Object -Last 1
    if (!$readyLine) { throw 'Staging did not return a ready directory.' }
    $stagingPath = $readyLine.Substring($readyPrefix.Length)
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot/PublishSymbols.ps1" -StagingDirectory $stagingPath -Store $Store
    if ($LASTEXITCODE -ne 0) { throw "Publish failed. Retry PublishSymbols.ps1 with: $stagingPath" }
}
catch {
    [Console]::Error.WriteLine($_.Exception.Message)
    exit 1
}
