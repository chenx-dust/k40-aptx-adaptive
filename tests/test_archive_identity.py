from pathlib import Path
import tempfile
import zipfile
from archive_identity import content_identity

with tempfile.TemporaryDirectory() as temp:
    root = Path(temp)
    def make(name, entries, compression=zipfile.ZIP_STORED):
        path = root / name
        with zipfile.ZipFile(path, 'w', compression) as archive:
            archive.comment = name.encode()
            for key, value in entries: archive.writestr(key, value)
        return path
    payload = [('classes.dex', b'dex contents'), ('res/raw/item', b'resource')]
    baseline = content_identity(make('original.jar', payload))
    repacked = make('resigned.jar', list(reversed(payload)) + [
        ('META-INF/CERT.RSA', b'different signing certificate'),
        ('META-INF/CERT.SF', b'new signature'),
        ('META-INF/MANIFEST.MF', b'Manifest-Version: 1.0\r\nCreated-By: signer\r\n\r\nName: classes.dex\r\nSHA-256-Digest: value\r\n\r\n')], zipfile.ZIP_DEFLATED)
    assert content_identity(repacked) == baseline
    for name, entries in [
        ('code.jar', [('classes.dex', b'changed')] + payload[1:]),
        ('resource.jar', payload[:1] + [('res/raw/item', b'changed')]),
        ('added.jar', payload + [('classes2.dex', b'new code')]),
        ('classpath.jar', payload + [('META-INF/MANIFEST.MF', b'Manifest-Version: 1.0\nClass-Path: other.jar\n')]),
        ('metadata.jar', payload + [('META-INF/services/example', b'provider')]),
    ]:
        assert content_identity(make(name, entries)) != baseline, name
    for name, entries in [('duplicate.jar', payload + payload[:1]),
                          ('traversal.jar', payload + [('../outside', b'x')])]:
        try: content_identity(make(name, entries))
        except ValueError: pass
        else: raise AssertionError(name)
print('PASS: repack/signature-record changes accepted; code/resources/manifest semantics/extra DEX changes rejected; malformed paths and duplicates rejected.')
