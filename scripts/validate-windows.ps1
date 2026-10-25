$ErrorActionPreference = 'Stop'
Set-Location (Split-Path $PSScriptRoot -Parent)
$path = Join-Path (Get-Location) 'build-win/JMoney_artefacts/Release/VST3/J Money - Vocal Intelligence EQ.vst3'
New-Item -ItemType Directory -Force build-win/validation-tools | Out-Null
New-Item -ItemType Directory -Force build-win/validation-results | Out-Null
Invoke-WebRequest 'https://github.com/Tracktion/pluginval/releases/download/v1.0.4/pluginval_Windows.zip' -OutFile build-win/validation-tools/pluginval.zip
Expand-Archive build-win/validation-tools/pluginval.zip build-win/validation-tools -Force
$validator = Get-ChildItem build-win/validation-tools -Filter pluginval.exe -Recurse | Select-Object -First 1
if (!$validator) { throw 'pluginval executable not found' }
$resultDir = (Resolve-Path build-win/validation-results).Path
$logPath = Join-Path $resultDir 'native-pluginval.txt'
if (Test-Path $logPath) { Remove-Item $logPath }
$arguments = @('--strictness-level','5','--timeout-ms','60000','--random-seed','12345','--output-dir',('"'+$resultDir+'"'),'--output-filename','native-pluginval.txt','--validate',('"'+$path+'"'))
$process = Start-Process -FilePath $validator.FullName -ArgumentList $arguments -PassThru
if (!$process.WaitForExit(120000)) { Stop-Process -Id $process.Id -Force; throw 'pluginval exceeded the external two-minute timeout' }
$process.Refresh()
if ($process.ExitCode -ne 0) { throw "pluginval failed with exit code $($process.ExitCode)" }
if (!(Test-Path $logPath)) { throw 'pluginval did not produce a test log' }
$log = Get-Content $logPath -Raw
Write-Output $log
if ($log -notmatch 'Completed tests in pluginval / Editor' -or $log -notmatch 'SUCCESS' -or $log -match '\*\*\* FAILED|FAILURE') {
    throw 'pluginval log does not confirm completed GUI validation'
}
