"""Patch three method entry points in the exact phone JARs; no generic framework replacement."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import subprocess
import zipfile
from archive_identity import content_identity

work = Path(__file__).resolve().parent
parser = argparse.ArgumentParser()
parser.add_argument('--build', choices=['1790089056', '1792813374'], default='1790089056')
parser.add_argument('--input-dir', type=Path, required=True)
parser.add_argument('--output-dir', type=Path, required=True)
parser.add_argument('--android-tree', type=Path, required=True)
parser.add_argument('--scratch-dir', type=Path, required=True)
args = parser.parse_args()
build = args.build
current = build == '1792813374'
device = work / ('device-' + build if current else 'device')
if args.input_dir: device = args.input_dir
tree = args.android_tree.resolve()
scratch = args.scratch_dir.resolve()
if scratch.exists() and any(scratch.iterdir()):
    raise SystemExit('Use an empty scratch directory to avoid stale disassembly')
out = work / ('module-src/jars-' + build if current else 'module-src/jars')
if args.output_dir: out = args.output_dir
out.mkdir(parents=True, exist_ok=True)
env = os.environ.copy()
env['PATH'] = str(tree / 'prebuilts/jdk/jdk21/linux-x86/bin') + ':' + env['PATH']
baksmali = tree / 'out/host/linux-x86/bin/smali-baksmali'
smali = tree / 'out/host/linux-x86/bin/android-smali'

forward = '''    const-wide v0, 0xad00d7ffL
    cmp-long v0, p0, v0
    if-nez v0, :k40_original
    const v0, 0x27000000
    return v0
    :k40_original
'''
reverse = '''    const v0, 0x27000000
    if-ne p0, v0, :k40_original
    const-wide v0, 0xad00d7ffL
    return-wide v0
    :k40_original
'''
create = '''    const-wide v0, 0xad00d7ffL
    cmp-long v2, p0, v0
    if-nez v2, :k40_original
    new-instance v2, Landroid/bluetooth/BluetoothCodecType;
    const v3, 0xf4240
    const-string v4, "AptX Adaptive"
    invoke-direct {v2, v3, v0, v1, v4}, Landroid/bluetooth/BluetoothCodecType;-><init>(IJLjava/lang/String;)V
    return-object v2
    :k40_original
'''
jobs = [
    ('framework.jar', ('f5ad8f676e190ad5de801f7be2b954e27af295d1dd65d6de4daba3f251dd2230' if current else
                       'f9fa4d3b8ea4d2cef82dcfe8aa6dbf0e7103a52d57de7c510ba1eb3030180cb9'),
     'classes2.dex', 'android/media/AudioSystem.smali', [
         ('bluetoothA2dpCodecToAudioFormat(J)I', 4, forward),
         ('audioFormatToBluetoothA2dpSourceCodec(I)J', 2, reverse)]),
    ('framework-bluetooth.jar', 'f752b5969e654bef6b0d517bb7ea7b8489213f619a475d11d4cb4544e56bc203',
     'classes.dex', 'android/bluetooth/BluetoothCodecType.smali', [
         ('createFromId(J)Landroid/bluetooth/BluetoothCodecType;', 5, create)]),
]
def digest(data):
    return hashlib.sha256(data).hexdigest()
def disassemble(dex, directory):
    subprocess.run([str(baksmali), 'disassemble', '-j', '4', '--use-locals',
                    '--sequential-labels', '-o', str(directory), str(dex)], env=env, check=True)
def files(directory):
    return {str(p.relative_to(directory)): p.read_text() for p in directory.rglob('*.smali')}
def methods(text):
    return dict(re.findall(r'^\.method [^\n]*? ([^ ]+\([^\n]+)\n(.*?)^\.end method',
                           text, flags=re.M | re.S))
def normalized(text):
    # DEX may omit trailing static initial values equal to the type default.
    # The assembler drops explicit false for some static booleans; both mean
    # false. Do not normalize instructions, annotations or nonzero values.
    return re.sub(r'^(\.field [^\n]*\bstatic\b[^\n]*:Z) = false$', r'\1', text, flags=re.M)
manifest = {}
for jar, expected, entry, cls, edits in jobs:
    source = device / jar
    actual_digest = digest(source.read_bytes())
    profile = json.loads((work / 'archive-content-profiles.json').read_text())[build][jar]
    assert profile['audited_archive_sha256'] == expected
    identity = content_identity(source)
    assert identity == profile['identity'], f'Unreviewed DEX/resources: {jar}'
    stage = scratch / jar
    stage.mkdir(parents=True, exist_ok=True)
    original = stage / 'original.dex'
    with zipfile.ZipFile(source) as z:
        original.write_bytes(z.read(entry))
    # The input is DEX039 with hidden-API metadata. This assembler requires
    # API29+ to retain that metadata and emits DEX040, supported by Android17.
    assert original.read_bytes()[:8] == b'dex\n039\x00'
    before_dir, after_dir = stage / 'before', stage / 'after'
    disassemble(original, before_dir)
    original_files = files(before_dir)
    target = before_dir / cls
    text = original_files[cls]
    for signature, locals_count, code in edits:
        pattern = r'(^\.method [^\n]* ' + re.escape(signature) + r'\n)(.*?)(^\.end method)'
        matches = list(re.finditer(pattern, text, flags=re.M | re.S))
        assert len(matches) == 1, signature
        m = matches[0]
        body = m.group(2)
        assert f'    .locals {locals_count}\n' in body and ':k40_original' not in body
        # Insert after method annotations and before the original first source line.
        pos = body.index('    .line ')
        body = body[:pos] + code + '\n' + body[pos:]
        text = text[:m.start(2)] + body + text[m.end(2):]
    before_methods, after_methods = methods(original_files[cls]), methods(text)
    assert before_methods.keys() == after_methods.keys()
    assert {k for k in before_methods if before_methods[k] != after_methods[k]} == {e[0] for e in edits}
    target.write_text(text)
    patched = stage / 'patched.dex'
    patched.unlink(missing_ok=True)
    assembled = subprocess.run([str(smali), 'assemble', '-a', '34', '-j', '4', '-o', str(patched),
                               str(before_dir)], env=env, capture_output=True, text=True)
    (stage / 'assemble.log').write_text(assembled.stdout + assembled.stderr)
    assert assembled.returncode == 0 and patched.exists(), (stage / 'assemble.log')
    assert patched.read_bytes()[:8] == b'dex\n040\x00'
    disassemble(patched, after_dir)
    roundtrip = files(after_dir)
    assert original_files.keys() == roundtrip.keys()
    # Reassembly must preserve every unrelated class, including hidden-API metadata.
    differences = [k for k in original_files if normalized(original_files[k]) != normalized(roundtrip[k])]
    assert differences == [cls], differences[:20]
    rt_methods = methods(roundtrip[cls])
    assert before_methods.keys() == rt_methods.keys()
    assert {k for k in before_methods if before_methods[k] != rt_methods[k]} == {e[0] for e in edits}
    output = out / jar
    with zipfile.ZipFile(source) as z, zipfile.ZipFile(output, 'w') as dest:
        for item in z.infolist():
            dest.writestr(item, patched.read_bytes() if item.filename == entry else z.read(item))
    aligned = out / (jar + '.aligned')
    zipalign = tree / 'out/host/linux-x86/bin/zipalign'
    subprocess.run([str(zipalign), '-f', '4', str(output), str(aligned)], check=True)
    aligned.replace(output)
    subprocess.run([str(zipalign), '-c', '4', str(output)], check=True)
    with zipfile.ZipFile(source) as z, zipfile.ZipFile(output) as dest:
        assert z.namelist() == dest.namelist()
        assert [n for n in z.namelist() if z.read(n) != dest.read(n)] == [entry]
        assert dest.testzip() is None
    manifest[jar] = dict(input_sha256=actual_digest, audited_input_sha256=expected,
                         content_sha256=identity['sha256'], output_sha256=digest(output.read_bytes()),
                         changed_dex=entry, input_dex_version=39, output_dex_version=40,
                         changed_methods=[e[0] for e in edits],
                         unchanged_classes=len(original_files)-1,
                         validation='assembled and disassembled; unrelated classes/methods unchanged; not ART runtime verified')
    print(jar, 'patched and verified:', manifest[jar]['changed_methods'], flush=True)
(out / 'patch-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
