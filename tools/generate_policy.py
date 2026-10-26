"""Add format-specific routes to the captured PixelOS policy, not stock MIUI."""
from pathlib import Path
import hashlib
import json
import xml.etree.ElementTree as ET
import argparse

parser = argparse.ArgumentParser()
parser.add_argument('source', type=Path)
parser.add_argument('output_dir', type=Path)
args = parser.parse_args()
source = args.source
data = source.read_text()
tree = ET.fromstring(data)
primary = tree.find('./modules/module[@name="primary"]')
assert primary is not None
mixes = {m.attrib['name'] for m in primary.findall('./mixPorts/mixPort')}
assert {'primary output', 'deep_buffer'} <= mixes
assert not any('A2DP' in d.attrib['type'] for d in primary.findall('./devicePorts/devicePort'))
ports, routes = [], []
for suffix in ('', '_HEADPHONES', '_SPEAKER'):
    tag = 'K40 Adaptive A2DP' + suffix
    ports.append(f'''                <devicePort tagName="{tag}" type="AUDIO_DEVICE_OUT_BLUETOOTH_A2DP{suffix}" role="sink"
                            encodedFormats="AUDIO_FORMAT_APTX_ADAPTIVE">
                    <profile name="" format="AUDIO_FORMAT_PCM_24_BIT_PACKED"
                             samplingRates="48000 96000" channelMasks="AUDIO_CHANNEL_OUT_STEREO"/>
                </devicePort>
''')
    routes.append(f'''                <route type="mix" sink="{tag}"
                       sources="primary output,deep_buffer"/>
''')
start = data.index('<module name="primary"')
end = data.index('</module>', start)
module = data[start:end]
assert module.count('</devicePorts>') == module.count('</routes>') == 1
module = module.replace('            </devicePorts>', ''.join(ports) + '            </devicePorts>')
module = module.replace('            </routes>', ''.join(routes) + '            </routes>')
result = data[:start] + module + data[end:]
updated = ET.fromstring(result)
assert len(updated.findall('./modules/module[@name="primary"]/devicePorts/devicePort')) == len(primary.findall('./devicePorts/devicePort')) + 3
# Preserve every existing device, route, mix port and the software Bluetooth include.
new_primary = updated.find('./modules/module[@name="primary"]')
for section in ('devicePorts', 'routes', 'mixPorts'):
    before = [ET.tostring(n).strip() for n in primary.find(section)]
    after = [ET.tostring(n).strip() for n in new_primary.find(section)]
    assert after[:len(before)] == before
assert data[:start] == result[:start] and data[end:] == result[result.index('</module>', start):]
out = args.output_dir
out.mkdir(parents=True, exist_ok=True)
(out / source.name).write_text(result)
(out / 'baseline.json').write_text(json.dumps({
    'input_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
    'adaptive_policy_format': '0x27000000',
    'stock_bridge_format': '0x25000000',
    'required_properties': {
        'ro.bluetooth.a2dp_offload.supported': 'true',
        'persist.bluetooth.a2dp_offload.disabled': 'false',
        'persist.vendor.audio.a2dp_codec_extensibility': 'false',
    },
    'status': 'candidate; AudioPolicy runtime routing not yet verified',
}, indent=2) + '\n')
print('Generated three Adaptive-only primary routes; existing policy entries preserved')
