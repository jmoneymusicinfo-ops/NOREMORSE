# Processing and engineering notes

## Manual and dynamic EQ

Each band has enabled, dynamic, audition, filter type, route, frequency, gain, Q, threshold, ratio, attack, release, knee, maximum range, and pass-filter slope parameters. Pass slopes cascade one to four second-order sections (12–48 dB/oct); slope has no meaning on other filters. Gain has no effect for pass/notch/band-pass types. Dynamic gain reduction is meaningful on bell and shelf stages; do not enable it on gain-independent types. These applicability restrictions are documented but the generic advanced parameter panel does not yet disable inapplicable controls.

RBJ-style biquad coefficients are bounded below 0.45 Fs. Gain, frequency and Q converge exponentially with a 20 ms time constant at a 32-sample coefficient cadence. A band-pass detector drives a soft-knee downward reduction, bounded by range, with first-order attack/release envelopes. Static gain is reduced by this envelope; this is not broadband compression. Detector routing currently uses linked L/R magnitude even for mid/side processing. Host parameters are read per block; coefficient updates are at 32-sample intervals, not sample-accurate VST3 points. Type/route/enabled changes are not yet crossfaded; change these while stopped. No callback files, locks, UI, or heap containers are used in project DSP. The integration harness instruments ordinary C++ new during processing; this does not intercept every allocator used by the host or framework.

## Vocal macros

| Macro | Implemented filter at full amount |
|---|---|
| rumble | 20–90 Hz high-pass, Q 0.8 (cutoff follows amount; zero disables) |
| mud | 320 Hz bell, Q 0.8, -5 dB |
| nasal | 1 kHz bell, Q 0.8, -4 dB |
| presence | 2.6 kHz bell, Q 0.8, +2 dB |
| harshness | 4.2 kHz dynamic bell, Q 0.8, up to -5 dB |
| sibilance | 7.2 kHz dynamic bell, Q 2, up to -5 dB |
| air | 12 kHz high shelf, Q 0.8, +2 dB |
| body | 160 Hz bell, Q 0.8, +2 dB |

Macro dynamic defaults: threshold -30 dBFS detector magnitude, ratio 2:1, knee 6 dB, attack 8 ms harshness / 2 ms sibilance, release 100 ms. Macro amounts scale gain/range. Manual bands remain independent. Rumble amount adjusts cutoff continuously.

## Learning and reference matching

This is DSP spectrum matching, not trained AI. A 32-band logarithmic analysis filter bank spans 60 Hz–16 kHz. It accumulates band energy over 10–20 accepted seconds (12 default) above approximately -70 dBFS smoothed input energy. Duration is the displayed confidence proxy. Near-flat unreliable measurements are rejected; this is only a basic safeguard, not a voice/noise classifier. Temporal power smoothing and Gaussian smoothing of log-frequency corrections limit fluctuations. Original default targets use a gentle falling spectrum with modest body/presence variations. Mean normalization removes loudness differences. Maximum cut/boost, frequency bounds, amount, and smoothing constrain the result. Empty frequency regions cannot receive positive correction. Continuous mode relearns successive 12-second windows; `adaptation_seconds` adjusts the acquisition window between 10 and 20 seconds.

Correction is realized by overlapping bell filters. Their summed response approximates the requested curve; it is not an exact inversion. Character preservation still requires listening. There is no robust breath classifier and no guarantee that stationary noise is rejected. Both pre- and post-processing band spectra are displayed.

Local WAV/AIFF/FLAC reference loading analyzes start/duration values in seconds from the advanced controls. Read/analysis happens on the UI thread, potentially pausing it. At least one second of above-floor reference samples is required. A normalized 32-bin target is retained in presets/session state. Press Learn on the source after loading a reference. Matching an isolated vocal to a complete mastered mix can badly mislead correction. Factory presets also use the reference-target storage for their original curves.

## Linear phase

When enabled, static **linked** manual bands are converted to a symmetric 513-tap Blackman-windowed FIR via a 1024-point inverse FFT of their combined magnitude. This is real linear-phase filtering, not a cosmetic switch. Group, learned correction, vocal macros, dynamic and channel-targeted bands remain minimum phase after this FIR. Thus the entire plug-in is not linear phase. A short FIR trades low-frequency resolution for CPU/latency; at 192 kHz bass matching is coarse. FIR coefficients are published with an atomic generation check and interpolated between audio blocks. Change phase mode with transport stopped: live latency transitions are not crossfaded.

Latency is 0 samples in minimum phase and 256 in linear phase; host notification occurs on the message timer. Dry bypass is delayed by 256 when active. Tail reporting uses a conservative pole-decay estimate for enabled manual filters, plus FIR delay and a four-second allowance for auxiliary stages. This is an estimate rather than a hard finite IIR cutoff. No explicit dry/wet control is currently exposed. Auto gain compares input/output moving energy, bounds compensation to ±6 dB, and smooths trim. Bypass returns delayed untrimmed dry audio with a short transition.

## Groups and host scheduling

Instances exchange only analysis snapshots through atomic-replaced XML files in the per-user temporary `JMoneyVEQ/<group hash>` directory. UUID identities distinguish duplicate display names. Peers expire after three seconds; destruction deletes the owner's snapshot. No socket, singleton, audio file, or blocking synchronization is used in the callback. A message-thread timer publishes/reads at 4 Hz. Ten fresh peers maximum are consumed, in sorted UUID order; additional instances are unsupported. Use unique group names per session because temporary groups are shared across host processes and sessions for that OS user.

Lower numerical tiers have priority. Above-floor spectral overlap produces a bounded, smoothed attenuation curve on lower-priority instances, using their own EQ stages. The sum of overlapping bells can exceed the per-bin maximum; that parameter bounds individual curve bins, not the total composite response. Only audio passed through an instance is visible. This is coarse spectral unmasking, not phase-coherent inter-track processing. Audition's process-isolation behavior has not been established here; file snapshots are intended to cover both arrangements but separate-process exchange and crashed-peer expiry are exercised by the integration harness; this does not establish Audition behavior.

Offline rendering freezes the most recent group curve, independent of wall-clock ordering during export. For repeatable exports, establish the curve during playback and save/render each stem with fixed settings; group reductions are serialized with session state. Missing peers decay back toward zero live. The group view includes preview cards, drag-to-tier assignment, double-click remote bypass, Learn All and a shared maximum-cut control. Remote commands support tier, bypass, Learn and group maximum cut; full remote band editing is not implemented. Commands use a last-write-wins mailbox, so rapid conflicting commands may be coalesced. There is no supported sidechain route.
