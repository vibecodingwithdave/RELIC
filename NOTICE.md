# Third-party notices

Relic itself is licensed under the GNU Affero General Public License, version 3 (`LICENSE`).

It is built with the following components, which are included under `third_party/` and keep their own licenses:

- **JUCE** (https://juce.com): dual-licensed under the AGPLv3 and the commercial JUCE licence. Relic uses the AGPLv3 option. See `third_party/JUCE/LICENSE.md`.
- **Steinberg VST3 SDK**, bundled inside JUCE: used under its GPLv3 option. See `third_party/JUCE/modules/juce_audio_processors/format_types/VST3_SDK/LICENSE.txt`.
- **Apple AudioUnitSDK**, bundled inside JUCE: Apache 2.0. See `third_party/JUCE/modules/juce_audio_plugin_client/AU/AudioUnitSDK/LICENSE.txt`.
- Other libraries bundled in JUCE (FLAC, Ogg Vorbis, libpng, libjpeg, zlib, HarfBuzz and others) carry their own permissive licenses; the full list is in `third_party/JUCE/LICENSE.md`.

VST is a trademark of Steinberg Media Technologies GmbH. Audio Units is a trademark of Apple Inc.
