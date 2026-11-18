<#
용도: 이전 Symbol Store를 재사용하거나 새 서버에 인증하고, debugger 설정을 적용해 Dump를 엽니다.
사용자가 편집할 설정 파일은 없습니다. 성공한 서버 경로만 HKCU에 기억하며 비밀번호는 저장하지 않습니다.
VS import용 임시 파일은 남겨두고 VS 실행 후 바로 반환합니다. Import한 VS 옵션은 이후에도 유지됩니다.
#>
#requires -Version 5.1
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$DumpPath,
    [Parameter(Mandatory)][string]$VisualStudioPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

try {
    # 1. 이전 주소를 보여준 뒤 선택합니다. 인증 성공 전에는 기억한 경로를 덮어쓰지 않습니다.
    $dumpFile = Get-Item -LiteralPath $DumpPath
    if ($dumpFile.PSIsContainer -or $dumpFile.Extension -ine '.dmp') {
        throw '하나의 .dmp 파일을 지정해 주세요.'
    }
    if (-not (Test-Path -LiteralPath $VisualStudioPath -PathType Leaf)) {
        throw 'Visual Studio 실행 파일을 찾지 못했습니다.'
    }

    $registryPath = 'HKCU:\Software\Hitori\CrashDebugging'
    $previousStore = Get-ItemPropertyValue -LiteralPath $registryPath -Name SymbolStore -ErrorAction SilentlyContinue
    $symbolStore = ''
    if ($previousStore) {
        Write-Host "이전에 사용한 Symbol Store: $previousStore"
        do {
            $reuse = Read-Host '이 서버를 사용하시겠습니까? [Y/n]'
        } while ($reuse -notmatch '^(?i:y|n)?$')
        if ($reuse -ine 'n') {
            $symbolStore = $previousStore
        }
    }
    if (-not $symbolStore) {
        $symbolStore = (Read-Host 'Symbol Store 경로 (예: \\서버PC\symbols)').Trim().TrimEnd('\')
    }
    if ($symbolStore -notmatch '^\\\\([^\\/:*?"<>|;\s]+)\\([^\\/:*?"<>|;]+)(\\[^/:*?"<>|;]*)?$') {
        throw 'Symbol Store는 \\서버\공유폴더 형태로 입력해 주세요.'
    }
    $sharePath = '\\' + $Matches[1] + '\' + $Matches[2]
    Write-Host "사용할 Symbol Store: $symbolStore"

    # 2. 이미 연결되어 있으면 그대로 사용합니다. 필요할 때만 Windows에 계정 인증을 맡깁니다.
    if (-not (Test-Path -LiteralPath $symbolStore -PathType Container -ErrorAction SilentlyContinue)) {
        $account = (Read-Host '공유 폴더 계정 (예: 서버PC\SymbolReader)').Trim()
        if (-not $account -or $account -match '["\r\n]') {
            throw '공유 폴더 계정을 입력해 주세요.'
        }
        # *는 net.exe가 비밀번호를 숨겨 입력받게 합니다. /savecred는 사용하지 않습니다.
        & net.exe use $sharePath '*' "/user:$account" /persistent:no
        if ($LASTEXITCODE -ne 0) {
            throw '공유 폴더 인증에 실패했습니다. 위 Windows 오류를 확인해 주세요. 기존 다른 계정 연결은 자동으로 해제하지 않습니다.'
        }
        if (-not (Test-Path -LiteralPath $symbolStore -PathType Container)) {
            throw '인증 후에도 Symbol Store에 접근할 수 없습니다. 경로와 읽기 권한을 확인해 주세요.'
        }
    }
    New-Item -Path $registryPath -Force | Out-Null
    New-ItemProperty -LiteralPath $registryPath -Name SymbolStore -Value $symbolStore -PropertyType String -Force | Out-Null

    # 3. 고정 옵션과 선택한 경로만 import합니다. 다른 debugger 옵션과 자동 symbol load 정책은 건드리지 않습니다.
    # 이 Setup을 포함한 clone 안에 cache를 준비합니다. /Saved/는 Git 추적 대상이 아닙니다.
    $repositoryRoot = Split-Path -Parent $PSScriptRoot
    $symbolCache = Join-Path $repositoryRoot 'Saved\CrashDebugging\SymbolCache'
    New-Item -ItemType Directory -Path $symbolCache -Force | Out-Null
    [xml]$settings = @'
<UserSettings>
  <ApplicationIdentity version="18.0" />
  <Debugger name="Debugger" Category="{EEDBF29A-5C8B-4E01-827C-263382C18CFE}" Package="{C9DD4A57-47FB-11D2-83E7-00C04F9902C1}" RegisteredName="Debugger" PackageName="CVSDebugPackage" />
</UserSettings>
'@
    $debuggerOptions = [ordered]@{
        UseSourceServer = '1'
        ShowSourceServerDiagnostics = '1'
        UseDocumentChecksum = '1'
        SymbolPath = $symbolStore
        SymbolPathState = '1'
        SymbolCacheDir = $symbolCache
    }
    foreach ($option in $debuggerOptions.GetEnumerator()) {
        $property = $settings.CreateElement('PropertyValue')
        $property.SetAttribute('name', $option.Key)
        $property.InnerText = $option.Value
        $settings.UserSettings.Debugger.AppendChild($property) | Out-Null
    }
    $settings.UserSettings.ApplicationIdentity.SetAttribute('version', [string]([Diagnostics.FileVersionInfo]::GetVersionInfo($VisualStudioPath).FileMajorPart) + '.0')
    $settingsPath = Join-Path ([IO.Path]::GetTempPath()) ('HitoriCrash-' + [guid]::NewGuid().ToString('N') + '.vssettings')
    $settings.Save($settingsPath)

    # 4. 현재 process의 symbol 경로도 맞춰 기존 환경변수의 cache override를 피합니다.
    # Git helper 환경변수는 호출한 batch에서 상속합니다. Windows 사용자 환경변수는 변경하지 않습니다.
    $env:_NT_SYMBOL_PATH = "srv*$symbolCache*$symbolStore"
    $launchArguments = '"' + $dumpFile.FullName + '" /Command "Tools.ImportandExportSettings /import:' + $settingsPath + '"'
    Write-Host '새 VS에서 debugger 설정을 import합니다. 완료 후 Native Debugging을 시작해 주세요.'
    Write-Host "Symbol cache: $symbolCache"
    # VS가 비동기로 import하므로 파일을 즉시 삭제하지 않습니다. 비밀번호는 파일에 기록하지 않습니다.
    # VS는 환경변수 사본을 상속하므로 launcher가 종료되어도 분석에 필요한 경로는 유지됩니다.
    Start-Process -FilePath $VisualStudioPath -ArgumentList $launchArguments -WorkingDirectory ([IO.Path]::GetDirectoryName($settingsPath))
}
catch {
    Write-Host "Dump 열기 실패: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}
