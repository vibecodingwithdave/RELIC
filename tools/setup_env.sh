#!/bin/sh
# One-time test environment: a virtualenv with Pedalboard (hosts the plugin headless), numpy and scipy.
cd "$(dirname "$0")/.." && python3 -m venv .venv && .venv/bin/pip install --quiet pedalboard numpy scipy && echo "ready: use .venv/bin/python tools/verify.py"
