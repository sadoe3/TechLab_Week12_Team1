#requires -Version 5.1
[CmdletBinding()]
param(
    [string]$SdkRoot,
    [switch]$ValidateOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$downloadedInstaller = $null
$setupExitCode = 0

function Get-WindowsSdkRoot {
    foreach ($view in @([Microsoft.Win32.RegistryView]::Registry64, [Microsoft.Win32.RegistryView]::Registry32)) {
        $registry = [Microsoft.Win32.RegistryKey]::OpenBaseKey('LocalMachine', $view)
        try {
            $key = $registry.OpenSubKey('SOFTWARE\Microsoft\Windows Kits\Installed Roots')
            if ($null -ne $key) {
                try {
                    $root = $key.GetValue('KitsRoot10')
                    if ($root -and (Test-Path -LiteralPath $root -PathType Container)) { return $root }
                }
                finally { $key.Dispose() }
            }
        }
        finally { $registry.Dispose() }
    }
    return Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10'
}

function Get-CachedSdkInstaller {
    $installers = foreach ($view in @([Microsoft.Win32.RegistryView]::Registry64, [Microsoft.Win32.RegistryView]::Registry32)) {
        $registry = [Microsoft.Win32.RegistryKey]::OpenBaseKey('LocalMachine', $view)
        try {
            $uninstall = $registry.OpenSubKey('SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall')
            if ($null -eq $uninstall) { continue }
            try {
                foreach ($name in $uninstall.GetSubKeyNames()) {
                    # Some unrelated uninstall entries deny reads to standard users.
                    try { $entry = $uninstall.OpenSubKey($name) }
                    catch [System.Security.SecurityException] { continue }
                    catch [System.UnauthorizedAccessException] { continue }
                    if ($null -eq $entry) { continue }
                    try {
                        $displayName = [string]$entry.GetValue('DisplayName')
                        if ($displayName -notlike 'Windows Software Development Kit*') { continue }
                        $path = [string]$entry.GetValue('BundleCachePath')
                        $version = $null
                        if ($path -and (Test-Path -LiteralPath $path -PathType Leaf) -and
                            [version]::TryParse([string]$entry.GetValue('DisplayVersion'), [ref]$version)) {
                            [pscustomobject]@{ Path = $path; Version = $version }
                        }
                    }
                    finally { $entry.Dispose() }
                }
            }
            finally { $uninstall.Dispose() }
        }
        finally { $registry.Dispose() }
    }
    $latest = $installers | Sort-Object Version -Descending | Select-Object -First 1
    if ($null -ne $latest) { return $latest.Path }
}

function Test-NonemptyFile([string]$Path) {
    return (Test-Path -LiteralPath $Path -PathType Leaf) -and ((Get-Item -LiteralPath $Path).Length -gt 0)
}

try {
    if (-not [Environment]::Is64BitOperatingSystem) { throw 'This engine requires 64-bit Windows.' }
    if ([string]::IsNullOrWhiteSpace($SdkRoot)) { $SdkRoot = Get-WindowsSdkRoot }
    if (-not (Test-Path -LiteralPath $SdkRoot -PathType Container)) {
        throw 'Windows SDK not found. Install the C++ workload and Windows SDK through Visual Studio Installer, then retry.'
    }
    $SdkRoot = (Resolve-Path -LiteralPath $SdkRoot).ProviderPath

    $sdkVersion = $null
    $includeRoot = Join-Path $SdkRoot 'Include'
    if (Test-Path -LiteralPath $includeRoot -PathType Container) {
        foreach ($candidate in (Get-ChildItem -LiteralPath $includeRoot -Directory |
            Where-Object Name -Match '^10\.\d+\.\d+\.\d+$' |
            Sort-Object { [version]$_.Name } -Descending)) {
            $version = $candidate.Name
            if ((Test-NonemptyFile (Join-Path $candidate.FullName 'um\DbgHelp.h')) -and
                (Test-NonemptyFile (Join-Path $candidate.FullName 'um\minidumpapiset.h')) -and
                (Test-NonemptyFile (Join-Path $SdkRoot "Lib\$version\um\x64\Dbghelp.lib"))) {
                $sdkVersion = $version
                break
            }
        }
    }
    if ($null -eq $sdkVersion) {
        throw 'Windows SDK headers or x64 Dbghelp.lib are missing. Add the Windows SDK through Visual Studio Installer, then retry.'
    }

    $debugTools = Join-Path $SdkRoot 'Debuggers\x64'
    $requiredTools = @(
        'symstore.exe', 'symchk.exe', 'srcsrv\srctool.exe', 'srcsrv\pdbstr.exe',
        'dbghelp.dll', 'dbgcore.dll', 'symsrv.dll', 'srcsrv.dll'
    )
    $missing = @($requiredTools | Where-Object { -not (Test-NonemptyFile (Join-Path $debugTools $_)) })
    if ($missing.Count -gt 0) {
        if ($ValidateOnly) { throw "Missing Debugging Tools files: $($missing -join ', '). Run SetupCrashTools.bat to install them." }

        $installer = Get-CachedSdkInstaller
        if (-not $installer) {
            # Official serviced Windows SDK 26100 installer; verify its signature before executing.
            $downloadedInstaller = Join-Path ([IO.Path]::GetTempPath()) ("Hitori-WinSdk-{0}.exe" -f [guid]::NewGuid().ToString('N'))
            Write-Host 'Downloading the Windows SDK installer from Microsoft...'
            [Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
            Invoke-WebRequest -UseBasicParsing -Uri 'https://go.microsoft.com/fwlink/?linkid=2349110' -OutFile $downloadedInstaller
            $installer = $downloadedInstaller
        }

        $signature = Get-AuthenticodeSignature -LiteralPath $installer
        if ($signature.Status -ne 'Valid' -or $null -eq $signature.SignerCertificate -or
            $signature.SignerCertificate.Subject -notmatch '(^|,\s*)CN=Microsoft Corporation(,|$)') {
            throw "The SDK installer does not have a valid Microsoft signature: $installer"
        }

        Write-Host 'Adding Debugging Tools for Windows. Approve the Windows administrator prompt if shown.'
        $startArguments = @{
            FilePath = $installer
            ArgumentList = @('/features', 'OptionId.WindowsDesktopDebuggers', '/quiet', '/norestart')
            WindowStyle = 'Hidden'
            Wait = $true
            PassThru = $true
        }
        $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
        try {
            $principal = New-Object Security.Principal.WindowsPrincipal($identity)
            if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) { $startArguments.Verb = 'RunAs' }
        }
        finally { $identity.Dispose() }
        $installProcess = Start-Process @startArguments
        if ($installProcess.ExitCode -notin @(0, 3010)) {
            throw "Windows SDK installer failed with exit code $($installProcess.ExitCode)."
        }
        $setupExitCode = $installProcess.ExitCode
        $missing = @($requiredTools | Where-Object { -not (Test-NonemptyFile (Join-Path $debugTools $_)) })
        if ($missing.Count -gt 0) { throw "Setup finished but required files are still missing: $($missing -join ', ')." }
        foreach ($relative in @("Include\$sdkVersion\um\DbgHelp.h", "Include\$sdkVersion\um\minidumpapiset.h", "Lib\$sdkVersion\um\x64\Dbghelp.lib")) {
            if (-not (Test-NonemptyFile (Join-Path $SdkRoot $relative))) {
                throw "Setup finished but the existing SDK file is missing: $relative"
            }
        }
    }

    Write-Host "Windows SDK: $sdkVersion ($SdkRoot)"
    Write-Host "Debugging Tools: $debugTools"
    Write-Host 'Ready: x64 Dump API headers/library, symstore, symchk, srctool and pdbstr.'
    Write-Host 'No PATH changes or SDK copies into the repository were made.'
    if ($ValidateOnly) { Write-Host 'Validation only: nothing was installed or changed.' }
    if ($setupExitCode -eq 3010) { Write-Host 'Windows SDK reports that a restart is required. No automatic restart was performed.' }
}
catch {
    [Console]::Error.WriteLine("Crash tools setup failed: $($_.Exception.Message)")
    $setupExitCode = 1
}
finally {
    if ($downloadedInstaller -and (Test-Path -LiteralPath $downloadedInstaller -PathType Leaf)) {
        Remove-Item -LiteralPath $downloadedInstaller -Force -ErrorAction SilentlyContinue
    }
}
exit $setupExitCode
