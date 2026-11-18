<#
용도: debugger의 SrcSrv 요청을 받아 Git local clone에서 특정 commit의 source를 가져옵니다.
입력: clone 경로, commit, 파일 경로, PDB checksum과 source cache 저장 경로입니다.
처리: GitSource.ps1로 원본 bytes를 읽고 checksum을 확인한 뒤 cache 파일을 씁니다.
Clone의 branch나 작업 파일은 바꾸지 않으며, 없는 commit을 자동 fetch하지 않습니다.
이 파일과 GitSource.ps1은 분석 PC의 같은 helper 폴더에 둡니다.
#>
#requires -Version 5.1
# 1. SrcSrv가 PDB 규칙에서 해석한 source 식별 정보와 cache 목적지를 받습니다.
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Repository,
    [Parameter(Mandatory)][ValidatePattern('^[0-9a-f]{40}$')][string]$Commit,
    [Parameter(Mandatory)][string]$SourcePath,
    [Parameter(Mandatory)][ValidateSet('MD5', 'SHA1', 'SHA256')][string]$Algorithm,
    [Parameter(Mandatory)][ValidatePattern('^[0-9a-fA-F]+$')][string]$Checksum,
    [Parameter(Mandatory)][string]$Destination
)

# 2. 공통 함수를 불러옵니다. 여기서는 아직 Git 조회나 파일 생성을 하지 않습니다.
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/GitSource.ps1"
try {
    # 3. Git object를 읽고 PDB checksum을 먼저 검사해 잘못된 source 저장을 막습니다.
    $gitBytes = Get-GitSourceBytes $Repository $Commit $SourcePath
    $sourceBytes = Get-MatchingSourceBytes $gitBytes $Algorithm $Checksum
    # 4. 검증한 bytes만 cache에 저장합니다. Clone의 working tree는 수정하지 않습니다.
    $destinationPath = [IO.Path]::GetFullPath($Destination)
    [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($destinationPath)) | Out-Null
    [IO.File]::WriteAllBytes($destinationPath, $sourceBytes)
}
catch {
    # 5. 조회·검증·저장 오류를 SrcSrv에 실패로 전달합니다. 자동 fetch는 하지 않습니다.
    [Console]::Error.WriteLine("Source retrieval failed: $($_.Exception.Message)")
    exit 1
}
