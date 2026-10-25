# Licensing and dependency notices

Project source is offered under GNU AGPL version 3 (see LICENSE). No trademark rights to J Money, Sonible, Adobe, JUCE or Steinberg are granted. Confirm branding rights before public distribution.

Pinned framework: JUCE 8.0.6, commit 51a8a6d7aeae7326956d747737ccf1575e61e209. Inspected repository LICENSE.md and bundled VST3 SDK LICENSE.txt. JUCE modules are dual AGPLv3/commercial; this project selects the open-source AGPL route. Closed-source distribution requires an appropriate separate licensing review and commercial JUCE terms. The pinned SDK offers GPLv3 or proprietary Steinberg terms, not an assumed MIT license based on newer SDK announcements. This project uses the GPL-compatible open-source route; retain corresponding source and notices on distribution. See docs/licenses/JUCE.md and docs/licenses/VST3.txt.

JUCE's third-party dependencies have their own notices (including FreeType, HarfBuzz, SheenBidi, PNG, JPEG, FLAC, Ogg/Vorbis and zlib as enabled by platform). Preserve the full pinned JUCE dependency source and embedded notices with source distributions. Build scripts fetch this exact source; they do not vendor or alter these licenses. No Sonible code, assets, profiles or trained models are used.

GNU AGPL license text retrieved from the Nextcloud upstream COPYING mirror because gnu.org was unavailable from this machine. Text identifies GNU AGPL v3, 19 November 2007.

Validation tools (not linked into plug-in): pluginval 1.0.4 from the official Tracktion release; standalone Steinberg validator built from SDK commit 3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96. The newer validator SDK's root license is MIT (docs/licenses/ValidatorSDK.txt); this does not change the older SDK license bundled into the pinned JUCE build.

Windows release toolchain: LLVM/Clang/LLD 19.1.7 (Apache 2.0 with LLVM exceptions); Microsoft C++ headers/libraries 14.44.35207 and Windows SDK 10.0.26100.0, obtained from Microsoft Visual Studio 17.14.42 manifests with payload SHA-256 verification. Microsoft tools and SDK are not included in these archives. Microsoft C/C++ runtime portions are statically linked; applicable Microsoft redistribution terms and System Library exceptions apply. Build-tool license reviewed at https://go.microsoft.com/fwlink/?LinkId=2179911 (Visual Studio 2022 Diagnostic Build Tools). The build uses compiler/SDK components for Windows development; no compiler license or proprietary SDK is granted to recipients.

J Money 0.1.1 applies a Windows-only compatibility modification to JUCE 8.0.6: `Component::createNewPeer` defaults to the existing GDI Software Renderer (engine 0), rather than Direct2D (engine 1). CMake applies the exact checked replacement to a writable dependency source/install; original JUCE copyright and AGPL notices remain intact. This change is not an upstream JUCE release or a Sonible implementation.

Compatibility modifications dated 2026-10-24: the pinned JUCE Windows VBlank worker also checks its atomic exit flag outside the successful-driver-wait branch, allowing shutdown after driver wait failures. The modification is explicitly marked in the affected source. Original framework notices are retained.
