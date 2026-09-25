# Downloads MobileSAM ONNX weights and onnxruntime.dll for Magic Select.
# Weights are not committed. Reconfigure after this so the installer can copy the DLL.
# A normal rebuild copies the DLL into the rundir either way.

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$models = Join-Path $root "data\models"
$ortDir = Join-Path $root "third_party\onnxruntime"
New-Item -ItemType Directory -Force -Path $models, $ortDir | Out-Null

$enc = Join-Path $models "mobilesam.encoder.onnx"
$dec = Join-Path $models "mobilesam.decoder.onnx"
if (-not (Test-Path -LiteralPath $enc)) {
    Invoke-WebRequest -Uri "https://huggingface.co/PulpCut/mobilesam-onnx/resolve/main/mobilesam.encoder.onnx" -OutFile $enc
}
if (-not (Test-Path -LiteralPath $dec)) {
    Invoke-WebRequest -Uri "https://huggingface.co/PulpCut/mobilesam-onnx/resolve/main/mobilesam.decoder.onnx" -OutFile $dec
}

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

Write-Host "MobileSAM models: $models"
Write-Host "ONNX Runtime: $dll"
Write-Host "CPU build. A DirectML onnxruntime.dll dropped in the same folder is used automatically if it exports the DML provider."
