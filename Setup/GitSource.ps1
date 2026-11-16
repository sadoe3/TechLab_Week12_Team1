<#
용도: StageSymbols.ps1과 GetIndexedSource.ps1이 함께 사용하는 Git source 처리 함수 모음입니다.
git.exe에서 특정 commit의 파일을 byte 단위로 읽고, PDB checksum과 대조합니다.
Git 원본 또는 LF를 CRLF로 복원한 내용 중 checksum이 일치하는 bytes만 반환합니다.
단독 실행용 script가 아니며 두 호출자가 dot-sourcing으로 함수를 불러옵니다.
Checkout, 작업 파일 변경, cache 저장은 이 파일에서 수행하지 않습니다.
#>
Set-StrictMode -Version Latest

# 1. Git source 조회: working tree 대신 지정 commit의 blob을 읽습니다.
function Get-GitSourceBytes([string]$Repository, [string]$Commit, [string]$SourcePath) {
    # 1-1. 따옴표로 구성하는 Git 인자에서 허용하지 않는 입력을 거릅니다.
    if ($Commit -notmatch '^[0-9a-f]{40}$' -or $SourcePath -match '["\r\n]' -or $Repository -match '["\r\n]') {
        throw 'Invalid Git source arguments.'
    }

    # 1-2. git.exe를 직접 실행하고 source 출력과 오류 출력을 별도로 받습니다.
    $start = New-Object Diagnostics.ProcessStartInfo
    $start.FileName = 'git.exe'
    $start.Arguments = '-C "' + $Repository.TrimEnd('\') + '" cat-file blob "' + $Commit + ':' + $SourcePath + '"'
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    $process = [Diagnostics.Process]::Start($start)
    $buffer = New-Object IO.MemoryStream
    try {
        # 1-3. 오류 출력도 비우면서 stdout은 byte stream으로 읽어 encoding 변환을 피합니다.
        $errorTask = $process.StandardError.ReadToEndAsync()
        $process.StandardOutput.BaseStream.CopyTo($buffer)
        $process.WaitForExit()
        if ($process.ExitCode -ne 0) {
            throw "Git source unavailable ($Commit / $SourcePath). Fetch the commit into the analysis clone. $($errorTask.Result)"
        }
        # 앞 comma는 byte 배열을 개별 숫자로 펼치지 않고 한 객체로 반환하게 합니다.
        return ,$buffer.ToArray()
    }
    finally {
        # 1-4. 성공·실패와 관계없이 이 함수가 만든 buffer와 process 객체를 해제합니다.
        $buffer.Dispose()
        $process.Dispose()
    }
}

# 2. PDB가 지정한 알고리즘으로 source bytes의 checksum을 계산합니다.
function Get-SourceChecksum([byte[]]$Bytes, [string]$Algorithm) {
    $hasher = [Security.Cryptography.HashAlgorithm]::Create($Algorithm)
    if ($null -eq $hasher) { throw "Unsupported PDB checksum: $Algorithm" }
    try { return ([BitConverter]::ToString($hasher.ComputeHash($Bytes))).Replace('-', '') }
    finally { $hasher.Dispose() }
}

# 3. LF 앞에 필요한 CR만 추가합니다. 다른 bytes와 기존 CRLF는 유지합니다.
function ConvertTo-CrlfBytes([byte[]]$Bytes) {
    $buffer = New-Object IO.MemoryStream
    try {
        for ($index = 0; $index -lt $Bytes.Length; ++$index) {
            if ($Bytes[$index] -eq 10 -and ($index -eq 0 -or $Bytes[$index - 1] -ne 13)) {
                $buffer.WriteByte(13)
            }
            $buffer.WriteByte($Bytes[$index])
        }
        return ,$buffer.ToArray()
    }
    finally { $buffer.Dispose() }
}

# 4. 원본 → CRLF 복원 순서로 검사하고 PDB와 일치하는 bytes만 caller에 돌려줍니다.
function Get-MatchingSourceBytes([byte[]]$Bytes, [string]$Algorithm, [string]$Checksum) {
    if ((Get-SourceChecksum $Bytes $Algorithm) -eq $Checksum) { return ,$Bytes }
    $crlfBytes = ConvertTo-CrlfBytes $Bytes
    if ((Get-SourceChecksum $crlfBytes $Algorithm) -eq $Checksum) { return ,$crlfBytes }
    throw 'Git source does not match the PDB checksum (including LF/CRLF). Rebuild from the intended commit.'
}
