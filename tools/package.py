#!/usr/bin/env python3
"""Package a reviewed staging directory; never pull files or modify a device."""
import argparse
import hashlib
from pathlib import Path, PurePosixPath
import zipfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('staging', type=Path)
parser.add_argument('output', type=Path)
args = parser.parse_args()
stage = args.staging.resolve()
rows = [line.split('|') for line in (stage / 'files.manifest').read_text().splitlines()]
if len(rows) != 6 or any(len(row) != 5 for row in rows):
    raise SystemExit('Expected exactly six complete payload entries')
if len({row[1] for row in rows}) != 6:
    raise SystemExit('Duplicate mount target')
names = ['module.prop', 'skip_mount', 'common.sh', 'customize.sh', 'post-fs-data.sh',
         'service.sh', 'action.sh', 'README.txt', 'files.manifest', 'compatibility.manifest']
if not (stage / 'compatibility.manifest').read_text().strip():
    raise SystemExit('Missing audited compatibility inputs')
for source, target, baseline, digest, context in rows:
    path = PurePosixPath(source)
    if path.is_absolute() or '..' in path.parts or not source.startswith('payload/'):
        raise SystemExit('Invalid payload path')
    candidate = (stage / source).resolve()
    if not candidate.is_relative_to(stage): raise SystemExit('Payload escapes staging')
    if hashlib.sha256(candidate.read_bytes()).hexdigest() != digest:
        raise SystemExit('Payload hash mismatch: ' + source)
    names.append(source)
if len(names) != len(set(names)): raise SystemExit('Duplicate archive entry')
args.output.parent.mkdir(parents=True, exist_ok=True)
with zipfile.ZipFile(args.output, 'w', zipfile.ZIP_DEFLATED) as archive:
    for name in names:
        data = (stage / name).read_bytes()
        info = zipfile.ZipInfo(name, (2026, 10, 27, 0, 0, 0))
        info.create_system = 3
        info.external_attr = (0o100755 if name.endswith('.sh') else 0o100644) << 16
        info.compress_type = zipfile.ZIP_DEFLATED
        archive.writestr(info, data)
with zipfile.ZipFile(args.output) as archive:
    assert archive.testzip() is None and archive.namelist() == names
digest = hashlib.sha256(args.output.read_bytes()).hexdigest()
args.output.with_suffix(args.output.suffix + '.sha256').write_text(digest + '  ' + args.output.name + '\n')
print(digest, args.output)
