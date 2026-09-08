[CmdletBinding()]
param(
    [ValidateSet('x64')]
    [string] $Target = 'x64',
    [ValidateSet('Debug', 'RelWithDebInfo', 'Release', 'MinSizeRel')]
    [string] $Configuration = 'RelWithDebInfo'
)

$ErrorActionPreference = 'Stop'

if ( $DebugPreference -eq 'Continue' ) {
    $VerbosePreference = 'Continue'
    $InformationPreference = 'Continue'
}

if ( $env:CI -eq $null ) {
    throw "Package-Windows.ps1 requires CI environment"
}

if ( ! ( [System.Environment]::Is64BitOperatingSystem ) ) {
    throw "Packaging script requires a 64-bit system to build and run."
}

if ( $PSVersionTable.PSVersion -lt '7.2.0' ) {
    Write-Warning 'The packaging script requires PowerShell Core 7. Install or upgrade your PowerShell version: https://aka.ms/pscore6'
    exit 2
}

function Get-IsccPath {
    $cmd = Get-Command iscc -ErrorAction SilentlyContinue
    if ( $cmd ) {
        return $cmd.Source
    }

    $candidates = @(
        "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
        "${env:ProgramFiles}\Inno Setup 6\ISCC.exe"
    )
    foreach ( $path in $candidates ) {
        if ( Test-Path -Path $path ) {
            return $path
        }
    }

    Log-Information "Installing Inno Setup..."
    Invoke-External choco install innosetup --yes --no-progress

    foreach ( $path in $candidates ) {
        if ( Test-Path -Path $path ) {
            return $path
        }
    }

    throw "ISCC.exe not found after Inno Setup install"
}

function Package {
    trap {
        Write-Error $_
        exit 2
    }

    $ScriptHome = $PSScriptRoot
    $ProjectRoot = Resolve-Path -Path "$PSScriptRoot/../.."
    $BuildSpecFile = "${ProjectRoot}/buildspec.json"

    $UtilityFunctions = Get-ChildItem -Path $PSScriptRoot/utils.pwsh/*.ps1 -Recurse

    foreach( $Utility in $UtilityFunctions ) {
        Write-Debug "Loading $($Utility.FullName)"
        . $Utility.FullName
    }

    $BuildSpec = Get-Content -Path ${BuildSpecFile} -Raw | ConvertFrom-Json
    $ProductName = $BuildSpec.name
    $ProductVersion = $BuildSpec.version
    $DisplayName = $BuildSpec.displayName
    $Author = $BuildSpec.author
    $Website = $BuildSpec.website

    $OutputName = "${ProductName}-${ProductVersion}-windows-${Target}"
    $ReleaseDir = "${ProjectRoot}/release"
    $PluginDir = "${ReleaseDir}/${Configuration}/${ProductName}"

    $RemoveArgs = @{
        ErrorAction = 'SilentlyContinue'
        Path = @(
            "${ReleaseDir}/${ProductName}-*-windows-*.zip"
            "${ReleaseDir}/${ProductName}-*-windows-*.exe"
            "${ReleaseDir}/${ProductName}-*-source.zip"
        )
    }

    Remove-Item @RemoveArgs

    if ( ! ( Test-Path -Path $PluginDir ) ) {
        throw "Plugin install tree not found: ${PluginDir}"
    }

    Log-Group "Archiving portable plugin..."
    $CompressArgs = @{
        Path = $PluginDir
        CompressionLevel = 'Optimal'
        DestinationPath = "${ReleaseDir}/${OutputName}.zip"
        Verbose = ($Env:CI -ne $null)
    }
    Compress-Archive -Force @CompressArgs
    Log-Group

    Log-Group "Building installer..."
    $Iscc = Get-IsccPath
    $Iss = Join-Path $ProjectRoot "cmake\windows\resources\installer.iss"
    $Defines = @(
        "/DMyAppName=`"${DisplayName}`""
        "/DMyAppVersion=`"${ProductVersion}`""
        "/DMyAppPublisher=`"${Author}`""
        "/DMyAppURL=`"${Website}`""
        "/DPluginName=`"${ProductName}`""
        "/DSourceDir=`"${PluginDir}`""
        "/DOutputDir=`"${ReleaseDir}`""
        "/DOutputName=`"${OutputName}`""
        "/DLicenseFile=`"$(Join-Path $ProjectRoot 'LICENSE')`""
    )
    Invoke-External $Iscc @Defines $Iss
    Log-Group

    Log-Group "Archiving source..."
    $SourceZip = "${ReleaseDir}/${ProductName}-${ProductVersion}-source.zip"
    Push-Location $ProjectRoot
    try {
        Invoke-External git archive --format=zip --prefix="${ProductName}-${ProductVersion}/" --output="${SourceZip}" HEAD
    } finally {
        Pop-Location
    }
    Log-Group
}

Package
