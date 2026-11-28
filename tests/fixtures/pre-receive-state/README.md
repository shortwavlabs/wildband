# Pre-RECEIVE plugin states (test fixtures)

These are states saved by the PR #17 plugin build (branch `p1/plugin-lifecycle`), before RECEIVE and its MIDI mapping
existed. They are the host-side state blobs exactly as JUCE's plugin hosting returned them from a freshly constructed
instance at default parameters:
- `vst3.state`: a `VST3PluginState` document;
- `au.state`: an AU class-info property list.

Both contain the plugin's `<WILDBAND version="1" …>` element with the ten parameters, and **no** `receiveCc` or
`receiveNote` attributes. This was checked by decoding them (the VST3 one uses JUCE's base64 variant).

`tools/PluginCheck.cpp` (RX-25, run by `tools/run_plugin_lifecycle_check.sh`) restores them into the current build to
check that restoring an older state clears the RECEIVE mapping.

| | |
|---|---|
| Made | 2026-11-27, with `wildband_plugin_check vst3=<PR #17 WILDBAND.vst3> dumpstate=tests/fixtures/pre-receive-state`, the PR #17 AU installed |
| PR #17 VST3 executable | SHA-256 `10ec51442e403d68f7588ff8b589b5449bfe2e8ec816028b0c31656cf91a53c7` |
| PR #17 AU executable | SHA-256 `d5ce274ac61b4c9e402512f1167a87d0222d399329a12173f0d3cd5b556439ab` |

Neither executable contains the string `receiveCc`.
