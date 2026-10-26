"""Exercise the real loader with sandbox files and fake privileged commands.

This verifies transaction logic only, not Android mount/SELinux/boot behavior.
"""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

SOURCE = Path(__file__).resolve().parents[1] / 'module'
TARGETS = [
    ('/system/framework/framework.jar', 'system_file'),
    ('/apex/com.android.bt/javalib/framework-bluetooth.jar', 'system_file'),
    ('/apex/com.android.bt/lib64/libbluetooth_jni.so', 'system_lib_file'),
    ('/vendor/lib/btaudio_offload_if.so', 'vendor_file'),
    ('/vendor/lib/libbluetooth_audio_session_aidl.so', 'vendor_file'),
    ('/vendor/etc/audio_policy_configuration.xml', 'vendor_configs_file'),
]
MOCK = r'''#!/usr/bin/python3
import json, os, pathlib, shutil, sys
root = pathlib.Path(os.environ['TEST_ROOT'])
state_path = root / 'state.json'
s = json.loads(state_path.read_text())
cmd, args = pathlib.Path(sys.argv[0]).name, sys.argv[1:]
s['calls'].append([cmd] + args)
rc = 0
if cmd == 'readlink':
    if s['scenario'] == 'namespace_failure': rc = 1
    else: print('mnt:[test]')
elif cmd == 'getprop':
    defaults = {'ro.product.device': 'alioth', 'ro.build.version.sdk': '37',
                'ro.build.version.incremental': '1790089056'}
    if s['scenario'] == 'wrong_device': defaults['ro.product.device'] = 'other'
    if s['scenario'] == 'different_build': defaults['ro.build.version.incremental'] = '9999999999'
    print(s['props'].get(args[0], defaults.get(args[0], '')))
elif cmd == 'pidof':
    rc = 0 if s['scenario'] == 'late_consumer' else 1
elif cmd == 'chcon': pass
elif cmd == 'ls':
    path = args[-1]
    context = ('vendor_configs_file' if '/vendor/etc/' in path else
               'vendor_file' if '/vendor/lib/' in path else
               'system_lib_file' if '/lib64/' in path else 'system_file')
    if s['scenario'] == 'wrong_label': context = 'unexpected_file'
    print('u:object_r:' + context + ':s0 ' + path)
elif cmd == 'mount':
    s['mount_attempts'] += 1
    if s['scenario'] == 'mount_failure' and s['mount_attempts'] == 4: rc = 1
    else:
        src, dst = args[-2:]
        backup = root / ('backup' + str(s['mount_attempts']))
        shutil.copyfile(dst, backup)
        s['backups'][dst] = str(backup)
        shutil.copyfile(src, dst)
elif cmd == 'umount':
    shutil.copyfile(s['backups'][args[0]], args[0])
elif cmd == 'resetprop':
    s['property_attempts'] += 1
    if s['scenario'] == 'property_failure' and s['property_attempts'] == 2: rc = 1
    elif args[1] == '--delete': s['props'].pop(args[2], None)
    else: s['props'][args[1]] = args[2]
else: raise AssertionError(cmd)
state_path.write_text(json.dumps(s))
sys.exit(rc)
'''

def digest(data):
    return hashlib.sha256(data).hexdigest()

def run(scenario):
    with tempfile.TemporaryDirectory(prefix='k40-loader-') as temp:
        root = Path(temp)
        module = root / 'module'
        shutil.copytree(SOURCE, module)
        # Redirect literal target allowlists and paths to an isolated filesystem.
        for script in module.glob('*.sh'):
            content = script.read_text()
            for prefix in ('/system/framework/', '/apex/', '/vendor/'):
                content = content.replace(prefix, str(root / 'fs') + prefix)
            content = content.replace('/data/adb/modules/', str(root / 'installed') + '/')
            script.write_text(content)
        (module / 'payload').mkdir()
        dependency = root / 'fs/vendor/lib/dependency.so'
        dependency.parent.mkdir(parents=True, exist_ok=True)
        dependency.write_bytes(b'audited-dependency')
        (module / 'compatibility.manifest').write_text(str(dependency) + '|' + digest(dependency.read_bytes()) + '\n')
        if scenario == 'wrong_dependency': dependency.write_bytes(b'changed')
        if scenario == 'missing_compatibility': (module / 'compatibility.manifest').unlink()
        rows, paths = [], []
        for i, (target, context) in enumerate(TARGETS):
            path = root / 'fs' / target.lstrip('/')
            path.parent.mkdir(parents=True, exist_ok=True)
            before, after = f'original-{i}'.encode(), f'patched-{i}'.encode()
            path.write_bytes(before)
            payload = f'payload/file{i}'
            (module / payload).write_bytes(after)
            rows.append('|'.join([payload, str(path), digest(before), digest(after),
                                  f'u:object_r:{context}:s0']))
            paths.append(path)
        if scenario == 'duplicate': rows[-1] = rows[0]
        if scenario == 'wrong_hash': paths[0].write_bytes(b'other-ROM')
        if scenario == 'missing_apex': paths[1].unlink()
        if scenario == 'previous_failed_boot': (module / 'boot_pending').touch()
        (module / 'files.manifest').write_text('\n'.join(rows) + '\n')
        if scenario.startswith('upgrade_'):
            old = root / 'installed/k40_pixelos_adaptive'
            old.mkdir(parents=True)
            oldrows = rows.copy()
            for i, path in enumerate(paths): path.write_bytes(f'patched-{i}'.encode())
            if scenario == 'upgrade_wrong_baseline':
                parts = oldrows[0].split('|')
                parts[2] = '0' * 64
                oldrows[0] = '|'.join(parts)
            if scenario == 'upgrade_tampered_live': paths[0].write_bytes(b'unknown-overlay')
            (old / 'files.manifest').write_text('\n'.join(oldrows) + '\n')
        initial = {str(p): p.read_bytes() if p.exists() else None for p in paths}
        props = {'persist.bluetooth.a2dp_offload.disabled': 'true'}
        state_path = root / 'state.json'
        state_path.write_text(json.dumps(dict(scenario=scenario, calls=[], props=props,
            mount_attempts=0, property_attempts=0, backups={})))
        commands = root / 'bin'
        commands.mkdir()
        for cmd in ('readlink', 'getprop', 'pidof', 'chcon', 'mount', 'umount', 'resetprop', 'ls'):
            executable = commands / cmd
            executable.write_text(MOCK)
            executable.chmod(0o755)
        env = dict(os.environ, PATH=str(commands) + ':/usr/bin:/bin', TEST_ROOT=str(root))
        if scenario.startswith('upgrade_') and scenario != 'upgrade_boot_refused':
            result = subprocess.run(['/bin/sh', '-c',
                'MODDIR="$1"; K40_UPGRADE=1; . "$MODDIR/common.sh"; k40_identity && k40_verify',
                'test', str(module)], env=env, capture_output=True, timeout=10)
            assert (result.returncode == 0) == (scenario == 'upgrade_success'), scenario
            assert json.loads(state_path.read_text())['mount_attempts'] == 0
            print('PASS', scenario)
            return
        if scenario == 'upgrade_boot_refused': env['K40_UPGRADE'] = '1'
        result = subprocess.run(['/bin/sh', str(module / 'post-fs-data.sh')], env=env,
                                capture_output=True, text=True, timeout=10)
        state = json.loads(state_path.read_text())
        if scenario in ('success', 'different_build'):
            assert result.returncode == 0
            assert not (module / 'disable').exists()
            assert (module / 'boot_pending').exists()
            assert state['mount_attempts'] == 6
            assert all(p.read_bytes() == f'patched-{i}'.encode() for i, p in enumerate(paths))
            assert state['props'] == {'ro.bluetooth.a2dp_offload.supported': 'true',
                'persist.bluetooth.a2dp_offload.disabled': 'false',
                'persist.vendor.audio.a2dp_codec_extensibility': 'false'}
        else:
            assert (module / 'disable').exists(), scenario
            assert state['props'] == props, scenario
            assert all((p.read_bytes() if p.exists() else None) == initial[str(p)] for p in paths)
            mounts = [c[-1] for c in state['calls'] if c[0] == 'mount']
            unmounts = [c[-1] for c in state['calls'] if c[0] == 'umount']
            if scenario == 'mount_failure': mounts = mounts[:-1]
            assert unmounts == list(reversed(mounts)), (scenario, state)
            if scenario not in ('mount_failure', 'property_failure'):
                assert state['mount_attempts'] == 0
        print('PASS', scenario)

for scenario in ('success', 'different_build', 'wrong_dependency', 'missing_compatibility',
                 'upgrade_success', 'upgrade_wrong_baseline', 'upgrade_tampered_live', 'upgrade_boot_refused',
                 'wrong_device', 'wrong_hash', 'wrong_label', 'duplicate', 'missing_apex',
                 'late_consumer', 'previous_failed_boot', 'namespace_failure',
                 'mount_failure', 'property_failure'):
    run(scenario)
