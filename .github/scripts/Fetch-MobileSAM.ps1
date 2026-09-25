# Downloads onnxruntime.dll for Magic Select.
# The ONNX models ship in data/models. Reconfigure after this so the installer can copy the DLL.
# A normal rebuild copies the DLL into the rundir either way.

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$ortDir = Join-Path $root "third_party\onnxruntime"
New-Item -ItemType Directory -Force -Path $ortDir | Out-Null

$dll = Join-Path $ortDir "onnxruntime.dll"
if (-not (Test-Path -LiteralPath $dll)) {
    $zip = Join-Path $env:TEMP "onnxruntime-win-x64-1.30.0.zip"
    if (-not (Test-Path -LiteralPath $zip)) {
        Invoke-WebRequest -Uri "https://github.com/microsoft/onnxruntime/releases/download/v1.30.0/onnxruntime-win-x64-1.30.0.zip" -OutFile $zip
    }
    $extract = Join-Path $env:TEMP "onnxruntime-win-x64-1.30.0"
    if (-not (Test-Path -LiteralPath (Join-Path $extract "lib\onnxruntime.dll"))) {
        Expand-Archive -LiteralPath $zip -DestinationPath (Join-Path $env:TEMP "ort-extract") -Force
        $extract = Join-Path $env:TEMP "ort-extract\onnxruntime-win-x64-1.30.0"
    }
    Copy-Item (Join-Path $extract "lib\onnxruntime.dll") -Destination $dll
}

Write-Host "ONNX Runtime: $dll"
Write-Host "CPU build. A DirectML onnxruntime.dll dropped in the same folder is used automatically if it exports the DML provider."
