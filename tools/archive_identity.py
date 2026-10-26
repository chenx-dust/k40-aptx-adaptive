"""Compatibility identity of archive contents, not signer authenticity.

Ignore ZIP layout and signature records; retain DEX/resources and meaningful
manifest attributes. Never use this function to authenticate a downloaded APK.
"""
import hashlib
import json
from pathlib import PurePosixPath
import re
import zipfile

def manifest_identity(data):
    text = data.decode('utf-8').replace('\r\n', '\n').replace('\r', '\n')
    text = text.replace('\n ', '')  # Manifest continuation lines.
    sections = []
    for block in text.split('\n\n'):
        attributes = {}
        for line in block.splitlines():
            if not line: continue
            key, separator, value = line.partition(': ')
            if not separator: raise ValueError('Malformed manifest')
            key = key.lower()
            if key in attributes: raise ValueError('Duplicate manifest attribute')
            attributes[key] = value
        retained = {k:v for k,v in attributes.items()
                    if k not in ('created-by', 'signature-version') and
                    not re.fullmatch(r'(sha-?\d+|md5)-digest(?:-manifest(?:-main-attributes)?)?', k)}
        if retained.get('manifest-version') == '1.0': del retained['manifest-version']
        if set(retained) == {'name'}: retained = {}
        if retained: sections.append(retained)
    return json.dumps(sections, sort_keys=True, separators=(',', ':')).encode()

def content_identity(path):
    entries = {}
    with zipfile.ZipFile(path) as archive:
        seen = set()
        for info in archive.infolist():
            name = info.filename
            if name in seen: raise ValueError('Duplicate ZIP entry: ' + name)
            seen.add(name)
            if name.startswith('/') or '..' in PurePosixPath(name).parts or '\\' in name:
                raise ValueError('Unsafe ZIP entry: ' + name)
            if info.is_dir(): continue
            upper = name.upper()
            if re.fullmatch(r'META-INF/([^/]+\.(SF|RSA|DSA|EC)|SIG-[^/]+)', upper):
                continue
            data = archive.read(info)
            if upper == 'META-INF/MANIFEST.MF':
                data = manifest_identity(data)
                if data == b'[]': continue
                name = 'META-INF/MANIFEST.MF'
            if name in entries: raise ValueError('Ambiguous ZIP identity')
            entries[name] = hashlib.sha256(data).hexdigest()
    if not any(re.fullmatch(r'classes\d*\.dex', name) for name in entries):
        raise ValueError('No DEX payload')
    canonical = json.dumps(entries, sort_keys=True, separators=(',', ':')).encode()
    return dict(sha256=hashlib.sha256(canonical).hexdigest(), entries=entries)
