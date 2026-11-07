# The Butterfly Compressor

A stereo image spreader triggered by a sidechain. Everything between **Low Cut** and **High Cut** is split into up to 3 bands. Each band is widened toward its own **Width** setting (max 110%) only while its part of the sidechain signal is active.

## Controls
| Control | What it does |
|---|---|
| Band FROM / TO | Each band has its own frequency knobs. Neighbouring bands share an edge, so moving Band 1's TO also moves Band 2's FROM. |
| Low Cut / High Cut | Sets the frequency range that gets processed. Frequencies outside it pass through untouched. You can drag these on the spectrum. |
| X1 / X2 | Band crossover frequencies. You can drag these on the spectrum. |
| Mode | **M/S**: boosts or cuts only the Side signal. **L/R**: changes the stereo image while keeping L/R level roughly constant. |
| Attack / Release | How fast the sidechain envelope responds. |
| Band On | Turns a band on or off. A band that is off stays at 100% width. |
| Width (0–110%) | The width a band reaches when the sidechain fully triggers it. |
| Feed (−12…+36 dB) | How strongly the sidechain in that band drives the widening. |
| Trigger (SC / SYNC) | **SC**: the band follows the sidechain. **SYNC**: the band pulses to full width on each note division locked to the DAW tempo, then curves back to 100% (like Kickstart). It needs no sidechain, and keeps running at the host tempo while the transport is stopped. |
| Rate (1/64 – 1/1) | Note division for SYNC mode. Feed is not used in SYNC mode. Attack and Release smooth the pulse. |
| Output | Output gain. |

## Display
- **MONITOR (ALL / B1 / B2 / B3)**: picks what the spectrum and goniometer show. ALL overlays every band in its own colour. The 4 correlation meters (ALL, B1, B2, B3) are always visible.
- **Spread LEDs** on each band show how far the band is currently pushed from 100% toward its Width.
- **Spectrum**: Mid (filled), Side before processing (cyan) and Side after processing (magenta). The band areas glow while they are triggered, and each shows its live width %.
- **Phase meter**: a goniometer plus a correlation bar from −1 to +1. The bar turns red when the output goes out of phase.

## Building
JUCE 8 is downloaded automatically.
```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```
Output files are in `build/ButterflyCompressor_artefacts/Release/` (VST3 and Standalone, plus AU on macOS).
On every push, GitHub Actions (`.github/workflows/build.yml`) builds macOS (universal arm64/x86_64) and Windows and uploads the plugins.

To use the plugin, route a track to the plugin's sidechain input in your DAW.
