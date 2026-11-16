"""Slice marker snapping (zero crossing, tempo grid) and the 4 ms choke cut are covered by dsp_slice_snap_test.cpp (exact) and verify_chop_kit.py (plugin)."""
import sys, subprocess, tempfile
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as tmp:
    b = str(Path(tmp) / 'snap'); subprocess.run(['clang++', '-std=c++17', '-O1', str(ROOT / 'tools/dsp_slice_snap_test.cpp'), '-o', b], check=True)
    r = subprocess.run([b], capture_output=True, text=True); print(r.stdout.strip()); sys.exit(r.returncode)
