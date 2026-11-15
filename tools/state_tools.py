"""Edit a hosted plugin's saved state (JUCE VST3 wrapper) so tests can give it a sample file."""
import re, struct
import xml.etree.ElementTree as ET
CH = ".ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+"
def jdecode(s):
    size, rest = s.split('.', 1); size = int(size); bits = []
    for c in rest:
        v = CH.index(c); bits += [(v >> k) & 1 for k in range(6)]
    return bytes(sum(bits[i * 8 + k] << k for k in range(8)) for i in range(size))
def jencode(b):
    bits = [(byte >> k) & 1 for byte in b for k in range(8)]
    while len(bits) % 6: bits.append(0)
    return str(len(b)) + '.' + ''.join(CH[sum(bits[i + k] << k for k in range(6))] for i in range(0, len(bits), 6))
def unwrap(raw):
    xml = raw[8:].rstrip(b'\0').decode()
    comp = jdecode(re.search(r'<IComponent>(.*?)</IComponent>', xml, re.S).group(1).strip())
    size = struct.unpack('<I', comp[4:8])[0]
    return xml, comp, size
def rewrap(raw, comp_new):
    xml = re.sub(r'(<IComponent>).*?(</IComponent>)', lambda m: m.group(1) + jencode(comp_new) + m.group(2), raw[8:].rstrip(b'\0').decode(), flags=re.S)
    body = xml.encode()
    return b'VC2!' + struct.pack('<I', len(body)) + body + b'\0'
def edit_state(raw, params=None, sample_path=None, attrs=None):
    raw = bytes(raw); xml, comp, size = unwrap(raw)
    inner = comp[8:8 + size]; tail = comp[8 + size:]
    nul = inner.endswith(b'\0'); root = ET.fromstring(inner.rstrip(b'\0').decode())
    for pid, val in (params or {}).items():
        for e in root.findall('PARAM'):
            if e.get('id') == pid: e.set('value', repr(float(val)))
    if sample_path is not None: root.set('samplePath', sample_path)
    for k, v in (attrs or {}).items(): root.set(k, v)
    for k, v in (attrs or {}).items(): root.set(k, v)
    new_inner = ET.tostring(root, encoding='unicode').encode() + (b'\0' if nul else b'')
    return rewrap(raw, b'VC2!' + struct.pack('<I', len(new_inner)) + new_inner + tail)
