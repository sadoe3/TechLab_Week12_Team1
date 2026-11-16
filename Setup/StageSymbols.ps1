<#
용도: 기존 build의 EXE/PDB를 Build/SymbolStaging에 복사하고 게시할 PDB를 준비합니다.
실행: build 후 StageSymbols.ps1 -Configuration Debug로 별도 실행합니다.
처리: Git source와 PDB checksum 대조 → 복사본 PDB의 Source Indexing → EXE/PDB 일치 검사.
결과: indexing된 PDB, Build.json, 제외 source 목록과 검증 로그를 남깁니다.
기존 build 출력은 수정하지 않으며 Symbol Store 게시와 VS 자동 연결은 별도 단계입니다.
#>
#requires -Version 5.1
# 1. 처리할 build 구성과 Microsoft 도구 위치를 받고 공통 함수를 불러옵니다.
[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release', 'Shipping')][string]$Configuration = 'Debug',
    [string]$DebugTools = (Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\Debuggers\x64')
)

$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/GitSource.ps1"

# Git 텍스트 조회의 실패를 확인합니다. Source bytes 조회는 GitSource.ps1이 담당합니다.
function Invoke-GitText([string[]]$Arguments) {
    $output = @(& git -C $repository @Arguments)
    if ($LASTEXITCODE -ne 0) { throw "Git failed: $($Arguments -join ' ')" }
    return $output
}

# EXE가 요구하는 PDB인지 검사합니다. Source 내용 일치는 뒤에서 별도로 확인합니다.
function Test-SymbolPair([string]$Executable, [string]$SymbolDirectory) {
    # /s selects the staging directory; /v exposes the actual matching PDB path.
    # SymChk writes diagnostics to stderr even on success (Windows PowerShell 5.1).
    $ErrorActionPreference = 'Continue'
    $output = @(& "$DebugTools/symchk.exe" $Executable /s $SymbolDirectory /v 2>&1)
    $toolExitCode = $LASTEXITCODE
    $ErrorActionPreference = 'Stop'
    if ($toolExitCode -ne 0) { throw "EXE/PDB mismatch: $Executable`n$($output -join "`n")" }
    return ($output -join "`r`n")
}

try {
    # 2. 도구·commit·build 입력을 확인하고 기존 출력물을 읽을 경로를 정합니다.
    $repository = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
    foreach ($tool in @('symchk.exe', 'srcsrv/srctool.exe', 'srcsrv/pdbstr.exe')) {
        if (!(Test-Path -LiteralPath (Join-Path $DebugTools $tool))) { throw "Missing $tool. Run SetupCrashTools.bat." }
    }

    $commit = [string](Invoke-GitText @('rev-parse', 'HEAD'))
    $dirtyInputs = @(Invoke-GitText @('diff', '--name-only', '--ignore-submodules', 'HEAD', '--', 'Source', 'premake5.lua', '.gitattributes'))
    $newInputs = @(Invoke-GitText @('ls-files', '--others', '--exclude-standard', '--', 'Source'))
    if ($dirtyInputs.Count -gt 0 -or $newInputs.Count -gt 0) { throw 'Commit the engine build inputs and rebuild before staging symbols.' }

    # Git의 실제 경로 표기를 보관해 source 경로 매칭 뒤 대소문자를 복원합니다.
    $trackedFiles = @{}
    foreach ($entry in (Invoke-GitText @('-c', 'core.quotepath=false', 'ls-tree', '-r', '--name-only', $commit))) {
        $trackedFiles[$entry] = $entry
    }
    $outputDirectory = Join-Path $repository "Build/Bin/$Configuration-windows-x86_64"
    $moduleNames = @('HitoriEditor', 'CrashDumpWriter')
    foreach ($module in $moduleNames) {
        foreach ($extension in @('exe', 'pdb')) {
            if (!(Test-Path -LiteralPath "$outputDirectory/$module.$extension")) { throw "Build $Configuration first: missing $module.$extension" }
        }
    }

    # 3. 실행마다 독립 staging을 만들며 기존 build 출력과 이전 준비 결과를 보존합니다.
    $buildId = '{0}-{1}-{2}' -f $Configuration, $commit.Substring(0, 12), [guid]::NewGuid().ToString('N')
    $stagingDirectory = Join-Path $repository "Build/SymbolStaging/$buildId"
    [IO.Directory]::CreateDirectory($stagingDirectory) | Out-Null
    $excludedSources = New-Object 'Collections.Generic.List[string]'
    $moduleRecords = @()
    $utf8 = New-Object Text.UTF8Encoding($false)

    foreach ($module in $moduleNames) {
        $originalHashes = @{}
        # 4. 원본 hash를 기억한 뒤 EXE/PDB를 복사하고 같은 build의 조합인지 검사합니다.
        foreach ($extension in @('exe', 'pdb')) {
            $originalPath = "$outputDirectory/$module.$extension"
            $originalHashes[$extension] = (Get-FileHash -LiteralPath $originalPath -Algorithm SHA256).Hash
            Copy-Item -LiteralPath $originalPath -Destination $stagingDirectory
        }
        $stagedExe = "$stagingDirectory/$module.exe"
        $stagedPdb = "$stagingDirectory/$module.pdb"
        Test-SymbolPair $stagedExe $stagingDirectory | Set-Content -LiteralPath "$stagingDirectory/$module.symchk.log"

        # 5. PDB source 목록을 Git commit과 대조합니다. 제외와 checksum 불일치를 구분합니다.
        $rawSources = @(& "$DebugTools/srcsrv/srctool.exe" -r -h -z $stagedPdb)
        if ($LASTEXITCODE -ne 0) { throw "srctool failed: $module" }
        $sourceRows = New-Object 'Collections.Generic.List[string]'
        foreach ($line in $rawSources) {
            if ($line -notmatch '^(.*?)\t Checksum (MD5|SHA1|SHA256): ([0-9A-Fa-f]+)\s*$') {
                if ($line -match '^[A-Za-z]:\\') { $excludedSources.Add("$module`tNo checksum`t$line") }
                continue
            }
            $originalSource = $Matches[1]
            $algorithm = $Matches[2]
            $checksum = $Matches[3]
            if (!$originalSource.StartsWith($repository + '\', [StringComparison]::OrdinalIgnoreCase)) {
                $excludedSources.Add("$module`tExternal SDK/toolchain`t$originalSource")
                continue
            }
            $relativePath = $originalSource.Substring($repository.Length + 1).Replace('\', '/')
            if (!$trackedFiles.ContainsKey($relativePath)) {
                $excludedSources.Add("$module`tGenerated/untracked/submodule source`t$originalSource")
                continue
            }
            # Keep the original PDB path for lookup, but use Git's canonical case for retrieval.
            $relativePath = $trackedFiles[$relativePath]
            if ($originalSource -match '[*%"\r\n]' -or $relativePath -match '[*%"\r\n]') { throw "Unsupported SrcSrv path: $relativePath" }
            $gitBytes = Get-GitSourceBytes $repository $commit $relativePath
            try { $null = Get-MatchingSourceBytes $gitBytes $algorithm $checksum }
            catch { throw "$relativePath : $($_.Exception.Message)" }
            $sourceRows.Add("$originalSource*$commit*$relativePath*$algorithm*$checksum")
        }
        if ($sourceRows.Count -eq 0) { throw "No verifiable tracked source in $module PDB." }

        # 6. 분석 시 실행할 취득 규칙을 작성합니다. 여기서 retrieval script를 실행하지 않습니다.
        # Clone and helper locations are analysis-PC settings, not build-PC absolute paths.
        $stream = @(
            'SRCSRV: ini ------------------------------------------------',
            'VERSION=2', 'VERCTRL=Git',
            'SRCSRV: variables ------------------------------------------',
            # Content checksum separates versions without repeating long commit/source paths in the cache.
            'SRCSRVTRG=%targ%\Hitori\%var5%\%fnfile%(%var1%)',
            # %% leaves expansion of analysis-PC settings to cmd; SrcSrv already quotes srcsrvtrg.
            'SRCSRVCMD=cmd.exe /d /c powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%%HITORI_SOURCE_TOOLS%%\GetIndexedSource.ps1" -Repository "%%HITORI_SOURCE_REPO%%" -Commit "%var2%" -SourcePath "%var3%" -Algorithm "%var4%" -Checksum "%var5%" -Destination %srcsrvtrg%',
            'SRCSRV: source files ---------------------------------------'
        ) + $sourceRows.ToArray() + 'SRCSRV: end ------------------------------------------------'
        # 7. 복사본 PDB에 규칙을 넣고 readback과 EXE/PDB 대응을 다시 검사합니다.
        $streamPath = "$stagingDirectory/$module.srcsrv"
        [IO.File]::WriteAllText($streamPath, ($stream -join "`r`n") + "`r`n", $utf8)
        & "$DebugTools/srcsrv/pdbstr.exe" -w "-p:$stagedPdb" "-i:$streamPath" -s:srcsrv
        if ($LASTEXITCODE -ne 0) { throw "pdbstr write failed: $module" }
        $readback = @(& "$DebugTools/srcsrv/pdbstr.exe" -r "-p:$stagedPdb" -s:srcsrv)
        $readbackExitCode = $LASTEXITCODE
        # pdbstr's console output can expand CRLF into blank lines; compare stream records.
        $readback = @($readback | Where-Object { $_.Trim().Length -gt 0 })
        if ($readbackExitCode -ne 0 -or (($readback -join "`n").Trim() -ne ($stream -join "`n").Trim())) { throw "SrcSrv readback failed: $module" }
        Test-SymbolPair $stagedExe $stagingDirectory | Add-Content -LiteralPath "$stagingDirectory/$module.symchk.log"

        # 8. 처리 중 원본이 바뀌지 않았는지 확인하고 이번 module의 결과를 모읍니다.
        foreach ($extension in @('exe', 'pdb')) {
            if ((Get-FileHash -LiteralPath "$outputDirectory/$module.$extension").Hash -ne $originalHashes[$extension]) { throw 'Build output changed during staging. Stop concurrent builds and retry.' }
        }
        $moduleRecords += [ordered]@{
            Module = $module; IndexedSources = $sourceRows.Count
            ExeSHA256 = (Get-FileHash -LiteralPath $stagedExe).Hash
            PdbSHA256 = (Get-FileHash -LiteralPath $stagedPdb).Hash
            OriginalPdbSHA256 = $originalHashes['pdb']
        }
    }

    # 9. 모든 module 검사가 끝나야 완료 manifest를 만듭니다. 서버 게시는 C6의 책임입니다.
    [IO.File]::WriteAllLines("$stagingDirectory/ExcludedSources.txt", $excludedSources, $utf8)
    $manifest = [ordered]@{
        Status = 'IndexedAndVerified'; BuildId = $buildId; Commit = $commit
        Configuration = $Configuration; Platform = 'x64'; CreatedUtc = [DateTime]::UtcNow.ToString('o')
        SourceRepository = [string](Invoke-GitText @('remote', 'get-url', 'origin'))
        SourceRetrieval = 'Git local clone via GetIndexedSource.ps1; no checkout or automatic fetch'
        Modules = $moduleRecords; ExcludedSources = $excludedSources.Count
    }
    [IO.File]::WriteAllText("$stagingDirectory/Build.json", ($manifest | ConvertTo-Json -Depth 5), $utf8)
    Write-Output "Ready for symbol publishing (not published): $stagingDirectory"
}
catch {
    # 실패한 staging은 게시 준비 완료로 취급하지 않습니다.
    [Console]::Error.WriteLine("Symbol staging failed: $($_.Exception.Message)")
    # Build.json exists only on success. Keep partial files for diagnosis, never publish them.
    exit 1
}
