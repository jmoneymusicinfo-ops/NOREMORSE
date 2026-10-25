# Feature status — development checkpoint

This is a real DSP implementation and VST3 build, not a completed professional release. Validation results are recorded separately in VALIDATION.md. Implemented does not mean tested in Audition.

| Area | Implemented | Outstanding / limitation |
|---|---|---|
| Native VST3 | JUCE C++20/CMake, pinned commit, Linux and Windows x64 Release bundles; scripts and CI | Windows binaries tested under Wine; native Windows/Audition still untested |
| Manual EQ | 24 bands, seven types, pass slopes, L/R/M/S, enabled/audition, numeric automation, draggable graph nodes | Abrupt type/route/enable changes need crossfades; graph ignores routing and pass cascades |
| Dynamic EQ | Detector, threshold, ratio, attack/release, knee, bounded range | Linked detector for M/S; gain dynamics only affect bell/shelf; generic panel still exposes inapplicable parameters |
| Vocal EQ | Eight real macro stages | More listening/tuning across voices required |
| Learn | 32-band analysis, 10–20 s accepted audio, smoothing, bounded correction, frequency limits, amount, continuous windows, original profiles | Basic duration confidence, no robust voice/noise classifier; not trained AI |
| Reference | Local PCM/FLAC analysis, start/duration, target serialization, reuse through preset | Synchronous UI loading; no waveform region editor |
| Groups | Cross-process temp snapshots; ten peers; tiers; track/group/combined; bounded unmasking; preview cards; drag tier; remote bypass and Learn All | No full remote band editor; separate-process exchange/stale-peer recovery tested; Audition process investigation outstanding; limited overview overflow |
| Linear phase | Real symmetric 513-tap static-linked FIR; reported 256 samples; delayed dry bypass | Dynamics/macros/learn/groups remain minimum phase; mode changes require stopped transport; coarse bass resolution at high rates |
| Interface | Dark resizable editor; pre/post spectra; learned curve; nodes; factory/user presets; A/B; reset; undo/redo; meters/overload | Simple macro knobs plus generic advanced panel; no polished dedicated band panel; full keyboard audit and tooltip coverage outstanding |
| State | Stable IDs, all parameters, learned/reference/group curves, names | A/B slots live only while editor exists; undo semantics need verification |
| Factory presets | All 14 original presets and target curves in factory.json | Starting points, not Sonible presets; listening evaluation outstanding |
| Safety | Bounded compensation, manual trim, finite audio guards, denormal guards, smoothed gain/frequency/Q, latency-aware bypass | Broader allocation instrumentation, transport stress and live topology crossfade work remain |
| Validation | Portable DSP tests, full-processor integration harness, pluginval and Steinberg validator | Refer to actual completed results; Native Windows/Audition/i7 measurements outstanding |

No unrelated host tracks are accessible. EQ is not a limiter, noise remover, pitch corrector or broadband compressor. Screenshots supplied during implementation were used to understand single-track and group workflows; no reference graphics/assets are copied.
