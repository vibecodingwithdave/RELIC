# Art kit: what to generate for the Relic skin

The plugin draws its layout, text, graphs and moving parts in code. Art only supplies the surfaces. Today every surface is a flat color; each item below replaces one.

**Design size:** the layout lives on a 1484 x 1060 grid. Generate art at **2x (2968 x 2120 for the full window)** so it stays sharp on retina screens. PNG with transparency unless noted. Keep lighting consistent (top-left), and leave out all text, numbers and knob pointers, because those are drawn live.

## 1. Window and panels

| File | Size at 2x | Notes |
|---|---|---|
| `bg_plate.png` | 2968 x 2120 | Full-window stone backdrop with torches and props at the edges. No panels, no text, no controls. Keep the center calm. |
| `logo.png` | 1400 x 220 | "RELIC / The lost sound engine" banner, transparent background. Drawn at design position x 400, y 0. |
| `scene_*.jpg` | about 1100 x 1400 | The painted scenes, switchable with the arrows on the picture (design rect 124, 112, 384 x 558, cropped to fill). The bottom 150 px sits under the CRAFT / UNDO / REDO / CHAOS buttons, so keep the focal point above it. |
| `panel_frame.png` | 192 x 192 | Carved-stone panel frame, **9-slice** with 48 px corners. Used for the center, left, right and bottom panels. |
| `screen_frame.png` | 192 x 192 | Green display bezel, **9-slice**, 40 px corners. The glass interior is transparent. |

## 2. Controls

| File | Size at 2x | Notes |
|---|---|---|
| `knob_body.png` | 256 x 256 | Brass knob, **no pointer**. The pointer is drawn and rotated in code. |
| `knob_pointer.png` | 256 x 256 | The pointer alone, pointing straight up, centered. |
| `knob_ring.png` | 288 x 288 | Optional recessed track behind the knob. |
| `btn_stone.png` | 5 states, 160 x 80 each | One sheet in a row: normal, hover, pressed, lit, disabled. **9-slice** with 24 px corners. Used for tabs, small buttons, FILTER, LP/HP/BP/N. |
| `pad.png` | 5 states, 300 x 160 each | Effect pads, same five states. **9-slice**, 36 px corners. |
| `slider_track.png`, `slider_thumb.png` | 40 x 400, 56 x 40 | Vertical sliders (Vib, Porta). Thumb has no label. |
| `key_white.png`, `key_black.png` | 80 x 240, 52 x 150 | Keys, each with up and down states side by side. |
| `meter_seg.png` | 3 sprites, 40 x 14 | Green, amber, red segments, plus one dim "off" segment. |

## 3. Icons (96 x 96, transparent, centered)

Sidebar: `presets`, `chords`, `oneshots`, `textures`, `expand`, `history`, `save`, `init`.
Effect pads: `tremolo`, `delay`, `comp`, `crush`, `panner`, `reverb` (placeholder).
Other: `craft`, `undo`, `redo`, `chaos`, `help`, `settings`.

## 4. Font

One pixel or fantasy display font under an open license (for example the SIL Open Font License), as a `.ttf`. It gets embedded in the plugin. Please note the license name.

## 5. Where each piece goes (design units)

| Region | x, y, w, h |
|---|---|
| Banner | 0, 0, 1484, 88 |
| Scene | 112, 100, 408, 450 |
| Sidebar buttons | 22, 100 + 68 n, 86, 62 (n = 0..7) |
| Tabs | 548 + 147 n, 102, 138, 44 (n = 0..4) |
| Waveform screen | 548, 156, 724, 232 |
| Envelope screen | 548, 394, 360, 82 |
| Meter | 1176, 490, 92, 150 |
| Right column (master, EQ, filter, vib/porta) | 1290..1474, 70..950 |
| Effect pads (3 x 2) | 28 / 190 / 352, 790 and 878, 148 x 80 |
| Preset bar | 610, 694, 570, 44 |
| Keyboard | 636, 832, 570, 118 |

## 6. Prompting tips for consistent results
- Generate each sheet in one go, not piece by piece, so materials match (same brass, same stone, same green glass).
- Ask for flat orthographic front views for controls, with no perspective and no cast shadow on the background.
- Ask for 9-slice items with plain straight edges in the middle so they stretch cleanly.
- Keep text out: ask for "no lettering, no numerals".

Tell me when files are ready, and in what folder, and I'll wire them in one piece at a time. Anything missing keeps using the flat-color drawing.
