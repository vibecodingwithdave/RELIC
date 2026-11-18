#!/bin/sh
# Runs every verification suite against the built VST3 and prints the RESULT/FAIL lines. Build first (see AGENTS.md).
cd "$(dirname "$0")/.." || exit 1
PY=.venv/bin/python
for s in verify verify_sampler verify_sample_layers verify_texture_layer verify_mod_slots verify_chop verify_chop_kit verify_chop_manual verify_slice_snap verify_mod_curve verify_lfo_custom verify_loopfade verify_arp_chords verify_arp_run verify_arp_rhythm verify_arp_twohand verify_arp_chance verify_arp_group verify_arp_custom verify_arp_events verify_chord_custom verify_wavetable_user verify_tempo_sync verify_distortion verify_automation verify_stuck_notes verify_vibrato_env verify_preset_parts verify_preset_features verify_mod verify_modfx verify_fx2 verify_fx3 verify_env verify_osc23 verify_filter2 verify_sources verify_batch4 verify_craft verify_presets; do
  echo "## $s"; $PY tools/$s.py 2>&1 | grep -E "FAIL|RESULT|rendered|unknown parameter|skipping"
done
