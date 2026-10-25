# J Money — Vocal Intelligence EQ

Windows x64 VST3 development build for rap vocals.

## Downloads

- [Download the Windows plug-in ZIP](https://github.com/jmoneymusicinfo-ops/NOREMORSE/raw/refs/heads/main/downloads/JMoney-Vocal-Intelligence-EQ-Windows-x64.zip)
- [Download the complete source ZIP](https://github.com/jmoneymusicinfo-ops/NOREMORSE/raw/refs/heads/main/downloads/JMoney-Vocal-Intelligence-EQ-source.zip)
- [SHA-256 checksums](downloads/SHA256SUMS.txt)

If GitHub shows an archive preview message, click **Download raw** on the file page. Sign in to the GitHub account with repository access if required.

## Install

1. Extract the Windows ZIP.
2. Copy the entire **J Money - Vocal Intelligence EQ.vst3** folder to `C:\Program Files\Common Files\VST3`.
3. In Audition, open **Effects > Audio Plug-In Manager > Scan for Plug-ins**, then enable the effect.

Keep the bundle's `Contents` directory intact. The source ZIP is optional for installation. Full installation instructions and feature limitations are inside the archives.

## Validation and limitations

The real Windows x64 bundle was compiled using Clang/LLD and the Microsoft SDK. Windows processor/DSP tests, pluginval level 5, and Steinberg validator (47/47) passed under Wine. **Actual Windows desktop and Adobe Audition testing remain outstanding.** This is an unsigned development build with remaining feature limitations; it is not a finished commercial release or an identical recreation of Sonible software.

Source is under AGPLv3; dependency notices and pinned JUCE source are included in the source archive.
