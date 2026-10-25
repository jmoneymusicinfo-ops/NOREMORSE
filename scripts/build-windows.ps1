$ErrorActionPreference = 'Stop'
Set-Location (Split-Path $PSScriptRoot -Parent)
cmake -S . -B build-win -G 'Visual Studio 17 2022' -A x64
if ($LASTEXITCODE) { throw 'CMake configure failed' }
cmake --build build-win --config Release --parallel 2
if ($LASTEXITCODE) { throw 'Release build failed' }
ctest --test-dir build-win -C Release --output-on-failure
if ($LASTEXITCODE) { throw 'Validation failed' }
Write-Host 'VST3 bundle: build-win/JMoney_artefacts/Release/VST3/J Money - Vocal Intelligence EQ.vst3'
