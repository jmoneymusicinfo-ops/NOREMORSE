# Audition verification — NOT EXECUTED

Requires a real Windows x64 build and Adobe Audition installation. No Adobe compatibility result is implied by Linux or JUCE tests. Current Adobe online documentation requests returned HTTP 403; confirm current UI wording locally.

- [ ] Copy full Windows bundle to `C:\Program Files\Common Files\VST3`.
- [ ] Effects > Audio Plug-In Manager > Scan for Plug-ins; enable J Money entry.
- [ ] Record Audition version/build, Windows version, CPU, RAM and audio driver.
- [ ] Load on mono Waveform Effects Rack; verify all parameters and meters.
- [ ] Load on mono lead and stereo beat in Multitrack Effects Racks.
- [ ] Test 44.1, 48, 88.2, 96, 176.4 and 192 kHz with varied hardware buffer sizes.
- [ ] Learn from 12+ seconds of representative playback; silence must not complete learning.
- [ ] Save session with manual bands, learned/reference curves and group names; quit/reopen and compare.
- [ ] Automate frequency/gain/output/bypass; listen for clicks and confirm host envelopes.
- [ ] Compare bypass and A/B at matched level; verify latency in both phase modes after stopping/restarting transport.
- [ ] Load 2–10 grouped instances; lead Foreground, beat Background; confirm reduction only with actual overlapping peers.
- [ ] Determine instance process IDs using Windows tools; test across process isolation if supported.
- [ ] Delete/reload peers, duplicate names, reopen two sessions; verify no unintended cross-session group.
- [ ] Play group to establish decisions, then offline-export. Repeat and null/compare exports, including export with absent peers.
- [ ] Verify long tails, silence, low-level noise, overload indication, reference-file failures and invalid preset recovery.
- [ ] Profile on the intended i7-4790: default and all bands, minimum/linear phase, 1/10 instances, sample rate and block size recorded. Avoid assuming cloud Xeon timings apply.

Do not use on irreplaceable sessions until these checks pass. Back up sessions and compare rendered audio.
