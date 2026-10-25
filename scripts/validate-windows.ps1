$ErrorActionPreference = 'Stop'
Set-Location (Split-Path $PSScriptRoot -Parent)
$path = Join-Path (Get-Location) 'build-win/JMoney_artefacts/Release/VST3/J Money - Vocal Intelligence EQ.vst3'
New-Item -ItemType Directory -Force build-win/validation-tools | Out-Null
Invoke-WebRequest 'https://github.com/Tracktion/pluginval/releases/download/v1.0.4/pluginval_Windows.zip' -OutFile build-win/validation-tools/pluginval.zip
Expand-Archive build-win/validation-tools/pluginval.zip build-win/validation-tools -Force
$validator = Get-ChildItem build-win/validation-tools -Filter pluginval.exe -Recurse | Select-Object -First 1
if (!$validator) { throw 'pluginval executable not found' }
& $validator.FullName --strictness-level 5 --validate $path
if ($LASTEXITCODE) { throw 'pluginval failed' }
