#!/usr/bin/env python3
import os
from pathlib import Path
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
env = dict(os.environ, PYTHONPATH=str(root / 'tools'))
for script in ('test_archive_identity.py', 'test_module_loader.py'):
    subprocess.run([sys.executable, str(root / 'tests' / script)], env=env, check=True)
with tempfile.TemporaryDirectory(prefix='k40-tests-') as directory:
    sources = [root / 'src/bridge/adaptive_config_test.cpp',
               root / 'src/bridge/stream_ack_test.cpp', root / 'tests/test_qcom_packet.cpp']
    for source in sources:
        binary = Path(directory) / source.stem
        subprocess.run(['g++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-pthread',
                        '-fsanitize=address,undefined', '-I' + str(root), str(source), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
print('All bundled host tests passed. Device playback is a separate validation step.')
