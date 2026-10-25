# Editor compatibility update 0.1.1

A user reported Adobe Audition 2026 displaying a blank plug-in window, becoming unresponsive, and showing its application error dialog. The earlier 0.1.0 validation skipped GUI tests and did not establish editor compatibility. The precise Audition exception/faulting module has not been obtained, so these changes must not be described as a confirmed Audition crash fix.

Changes:

- Correct the pinned JUCE VBlank worker exit condition so a failed driver wait cannot prevent a requested shutdown. The failing-wait path was observed in the test environment; its relationship to the user's precise Audition exception is not confirmed.

- Square editor content: 920 × 920 initially, 920–1400 bounds, fixed 1:1 aspect constraint during host resizing. Audition's surrounding window chrome is controlled by the host.
- Apply a small, documented JUCE Windows peer-creation compatibility patch: select the Software Renderer (GDI) at native-window creation instead of constructing Direct2D first. Also select software mode for direct and host-embedded editor peers before painting. This reduces reliance on Direct2D drawing and graphics-driver compatibility. Windows fonts and native window APIs are still used.
- Construct the large generic advanced-parameter panel on request instead of creating hundreds of controls before the first window appears. Manual parameters remain available through the Advanced parameters button and host automation.
- Read group display names under their existing state mutex during editor initialization.
- Add an editor lifecycle regression that creates, paints, resizes and closes the native window five times, including host embedding, advanced-panel opening, square-size assertions and Windows renderer selection.

Install the new bundle with Audition completely closed. Remove the old J Money .vst3 folder from the VST3 installation directory before copying the new complete folder, then reopen Audition and rescan. Do not remove other manufacturers' plug-ins. The host may cache old entries until it has restarted.

Actual Audition testing on the affected PC is still required. If the error persists, the exception code and faulting module from Adobe's View report (or Windows application error details) are needed to investigate the specific failure.

Native Windows CI has now passed DSP, processor integration and the five-cycle editor lifecycle regression, including host embedding and constrained square client resizing. The actual published Windows VST3 also passed pluginval 1.0.4 at strictness 5 with GUI tests enabled, an explicit process wait, and a saved SUCCESS log. Native verification run: https://github.com/jmoneymusicinfo-ops/NOREMORSE/actions/runs/40173466975 .
