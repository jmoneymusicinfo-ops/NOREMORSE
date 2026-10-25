# Validation results

Development checkpoint: October 24, 2026 (America/Los_Angeles; raw tool logs use UTC).

## Actual builds and tests

- Linux x86_64 Release VST3: successfully compiled with GCC 14.2.0, CMake 4.4.4, pinned JUCE 8.0.6. This is a real VST3 bundle with module metadata and a native `.so`; it is **not a Windows plug-in**.
- CTest: **2/2 targets passed**, 10.09 seconds total in the final recorded run. DSP target makes 840072 numeric assertions (many are per-sample impulse stability checks, not independent test cases).
- Processor integration: passed mono/stereo; 44.1/48/88.2/96/176.4/192 kHz; 1/17/64/127/512/1024-sample float blocks; double silence; zero EQ identity; state and all 14 preset round trips; reference WAV analysis; nonzero learned curve serialization; invalid sample sanitization; low-level-noise learning rejection; actual FIR bell magnitude/symmetry; reported 256-sample latency and bypass impulse alignment; two- and ten-instance grouping; separate-process IPC and crashed-peer expiry; frozen offline reductions. Ordinary C++ `new` instrumentation reports zero allocations in instrumented processing calls. This is not a complete allocator/host stress audit.
- pluginval **1.0.4**, strictness **5**, fixed seed **12345**: **SUCCESS** on the final Linux bundle. GUI tests explicitly skipped on the headless machine. Its nested Steinberg step was skipped; the separate validator below ran independently.
- Steinberg VST3 validator, SDK commit `3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96`: **47 passed, 0 failed**. Module-info/factory compatibility warning retained in the log. Validator build's own 51 self-tests also passed.
- Windows x64 Release VST3: **successfully compiled** using Clang/LLD 19.1.7 with Microsoft ABI, Microsoft 14.44.35207 headers/libraries and SDK 10.0.26100.0. Static C/C++ runtime; PE imports checked: Windows system DLLs only. See WINDOWS-BUILD.md.
- Windows executable tests under Wine 10.0: **2/2 CTest targets passed**, 24.66 seconds. Same processor/DSP scenarios as Linux, including separate-process grouping and all ten peers.
- Windows pluginval 1.0.4 under Wine, strictness 5, seed 12345, GUI tests skipped: **SUCCESS** on the actual Windows bundle.
- Windows Steinberg validator under Wine: **47 passed, 0 failed**; validator self-tests **51 passed, 0 failed**. Its CMake post-build self-test initially attempted direct PE execution on Linux; it was instead executed through Wine.
- Native Windows desktop/Audition: **not executed**. Cross-build and Wine are not proof of Audition compatibility. Native Visual Studio build script and GitHub Actions workflow remain available; no remote CI run is claimed.
- Adobe Audition: **not tested**. See AUDITION.md; no host compatibility or process-isolation claim is made.

Final Linux module SHA-256: `da6a4501e1f53d481605038c7d68a48feb7412023889acb037904b2e94a671fc`.

Bundle: `build/JMoney_artefacts/Release/VST3/J Money - Vocal Intelligence EQ.vst3`.

## Performance evidence

Machine: virtualized Linux x86_64, Intel Xeon Platinum 8573C, 5 exposed vCPUs, about 33 GiB RAM; kernel `6.18.55`. GCC Release configuration. Host-style buffers generated and processed by the integration harness; no physical audio device or Audition. Timings include test signal generation and use shared-machine wall time, not measured audio-thread scheduling headroom.

| Configuration | Sample rate / block | Audio duration | Final wall time |
|---|---|---|---|
| One stereo instance, learned correction, minimum phase, manual bands/macros off | 48 kHz / 256 | 10 seconds | 0.390047 s |
| One stereo instance, 24 dynamic bands + learned correction + linear FIR | 48 kHz / 256 | 10 seconds | 0.816466 s |
| Ten stereo instances, established group unmasking curves, minimum phase | 48 kHz / 256 | 10 seconds each | 4.22579 s total |

IPC publication is outside the measured audio callbacks. Timings varied across runs; do not extrapolate them to the i7-4790, ten instances at 192 kHz, live monitoring, or Windows. Those measurements remain outstanding.

## Reproduction and evidence

Use README.md build commands and `ctest --test-dir build --output-on-failure`. The isolated cloud machine used a workspace-local development-header sysroot because system directories are read-only; standard Linux development-package installations need no such override. The local activation file is `/workspace/dependencies/build-env.sh` (machine-specific, not part of portable Windows instructions).

Raw logs: [build](results/build-linux.txt), [CTest](results/ctest.txt), [processor output](results/processor-tests.txt), [pluginval](results/pluginval-linux.txt), [Steinberg](results/steinberg-validator-linux.txt).

Remaining unvalidated areas include GUI rendering/keyboard behavior, undo semantics, live topology and latency changes, transport stress, reference-file error UX, exhaustive detector/tail behavior, native Windows sessions, Audition sessions/automation/export, and listening tests on representative rap recordings. See STATUS.md for implementation gaps. No Sonible equivalence or production-readiness claim is made.

## Windows delivery

Archive: `dist/JMoney-Vocal-Intelligence-EQ-Windows-x64.zip`. Copy its entire outer `.vst3` folder to `C:\Program Files\Common Files\VST3`.

Windows module SHA-256: `79c582f392694cb62b2a9d2a8beceda3dcdfd1e1979223356c4f5cad56c789f1`.

Exports verified: `GetPluginFactory`, `InitDll`, `ExitDll`; PE machine `IMAGE_FILE_MACHINE_AMD64`. Native Windows module metadata was generated by JUCE's Windows helper running through Wine.

[Windows CTest](results/ctest-windows-wine.txt), [processor output](results/processor-tests-windows-wine.txt), [Windows pluginval](results/pluginval-windows-wine.txt), [Windows Steinberg validator](results/steinberg-validator-windows-wine.txt), [PE imports/exports](results/windows-pe.txt). Wine benchmarks are test evidence only and are not i7-4790 or native-Windows performance claims.
