# Original factory presets

`presets/factory.json` is the authoritative table of actual parameter values and 32-bin original normalized target curves for all 14 named presets. Regenerate compiled data with `python scripts/generate_presets.py`. This is deterministic and does not download models. JSON macro values are 0–1 and resolve through the DSP table in DSP.md. Unspecified manual bands are disabled; unspecified dynamic band defaults are threshold -24 dBFS, ratio 2, attack 10 ms, release 120 ms, knee 6 dB, range 4 dB. Manual preset high-passes are static 12 dB/oct at Q .707.

All presets use conservative learned limits: -3/+1 dB, 1-unit Gaussian smoothing, amount .25 (beat: 0), upper limit 12 kHz. Low limit 90 Hz except Deep Voice 65 Hz. They use original targets with -3 dB/oct spectral tilt plus broad small body/presence preferences. These are starting points, not universal male voice corrections.

| Preset | Intention |
|---|---|
| J Money — Natural Rap Lead | Moderate cleanup, restrained presence, preserve body |
| Bay Area — Forward and Clear | More presence and dynamic harshness control |
| Deep Voice — Keep the Body | Lower correction boundary, no mandatory manual HP, added body |
| Bedroom Recording — Mud Control | Stronger 320 Hz cleanup; not room removal |
| Harsh Vocal — Smooth Presence | More dynamic 4.2 kHz attenuation |
| Sibilant Vocal — Controlled S | More dynamic 7.2 kHz attenuation |
| Thin Vocal — Gentle Warmth | Gentle 160 Hz warmth, no mandatory manual HP |
| Melodic Rap — Smooth and Airy | Modest air with dynamic high-frequency control |
| Fast Rap — Word Clarity | Presence/nasal cleanup for articulation |
| Rap Doubles — Behind the Lead | Higher HP and Background group tier |
| Ad-Libs — Bright and Tucked | Air and higher HP, Background group tier |
| Vocal Bus — Gentle Glue EQ | Small tonal adjustments only; no broadband compression |
| Beat — Make Room for Lead | Combined mode, Background tier, up to 2 dB group cuts; needs grouped foreground lead |
| Conservative — Cleanup Only | Mild cleanup with no presence/air boosts |

Save/load `.jmveq` through the editor; session state retains parameter IDs, learned/reference curves and group/name. Paths to reference audio are not required on reopen because curves are stored. A/B slots are temporary editor memory, not session state. Group reductions are saved; A/B slots still need session persistence work before release.
