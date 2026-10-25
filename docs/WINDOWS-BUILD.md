# Windows x64 build

The delivered plug-in is a native Windows PE/COFF x64 DLL in JUCE's standard VST3 bundle layout. It is compiled from the same processing source as the Linux build. It is not a renamed Linux file. Microsoft C/C++ runtimes are linked statically in this delivered configuration; imports are Windows system DLLs.

## Toolchain used

- JUCE 8.0.6, commit `51a8a6d7aeae7326956d747737ccf1575e61e209`, bundled VST 3.7.12 SDK.
- Clang/LLD 19.1.7, Microsoft ABI (`x86_64-pc-windows-msvc`), C++20, Release optimization, static runtime (`/MT`).
- Microsoft C++ headers/libraries 14.44.35207; Windows SDK 10.0.26100.0.
- Visual Studio installer manifest 17.14.42. Payload SHA-256 verification was retained.
- Microsoft components downloaded/prepared with `mstorsjo/msvc-wine` commit `514f8ea34842cd6d831804d0e9658d3a32870ae1`.
- Native Linux JUCE helper used during generation. Target Windows manifest helper ran through Wine 10.0.

MinGW was investigated and rejected by JUCE's explicit unsupported-toolchain guard. No JUCE guard or DSP code was patched to force it through. Clang is supported by JUCE on Windows. Microsoft tools/SDK are not redistributed in the plug-in archive.

## Native Windows reproduction

Use Visual Studio 2022 x64 C++ Build Tools, CMake, and Git, then run:

```powershell
./scripts/build-windows.ps1
./scripts/validate-windows.ps1
```

This uses the native Microsoft compiler and is a separate build path from the delivered cross-built artifact. GitHub Actions provides the same Windows recipe; no remote CI run is claimed.

## Linux-to-Windows reproduction

Install LLVM 19 (clang-cl, lld-link, llvm-lib, llvm-rc, llvm-mt), CMake and Ninja. Prepare the stated Microsoft headers/libraries using msvc-wine under its applicable Microsoft license terms. Build/install the pinned JUCE release natively first so its juceaide runs on the build machine. The standalone JUCE CMake install provides its native package. Then set:

```bash
export JM_MSVC_ROOT=/path/to/prepared/msvc
export JM_MSVC_WINE_TOOLS=/path/to/msvc-wine
export JM_NATIVE_JUCE_PACKAGE=/path/to/native/JUCE/lib/cmake/JUCE-8.0.6
export JM_WINE_RUNNER=/path/to/wine-launcher
# LLVM executables, CMake and Ninja must be on PATH.
./scripts/build-windows-clang.sh
```

The script copies the native JUCE CMake configuration beside itself with a `-cross` suffix, preserving the installation prefix and native juceaide while omitting prebuilt host LV2/VST3 loader exports. JUCE then compiles the target manifest helper. Use a private writable JUCE installation.

The script produces a case-insensitive Clang VFS overlay for SDK headers and adds equivalent-case library symlinks in the prepared SDK. It preserves header/library contents. This handles Linux's case-sensitive filesystem while respecting Windows include and library names. The Wine launcher must provide an initialized writable prefix and a display (Xvfb is sufficient for the automated headless tests).

Output: `build-windows-clang/JMoney_artefacts/Release/VST3/J Money - Vocal Intelligence EQ.vst3`.

## Verification scope

Windows executable tests and Windows pluginval ran under Wine on Linux, not on a Windows PC. This checks the Windows binary and ABI, but does not establish Adobe Audition compatibility. Actual Audition scanning, session recall, automation, group operation and export still need the checklist in AUDITION.md.
