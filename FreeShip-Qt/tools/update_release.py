"""Signed, content-defined incremental distribution of the existing setup EXE.

Only publishers need Python/cryptography. Clients use the standalone .NET helper.
No private key is written into the release directory or the application bundle.
"""
import argparse, base64, gzip, hashlib, json, os, re
from datetime import datetime, timedelta, timezone
from pathlib import Path
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import padding, rsa

FAMILY = 'ad4aa28d-89c6-5dad-8ead-46843a810838'
MIN_CHUNK, MAX_CHUNK, MASK = 16384, 262144, 65535

def gear_table():
    value = 0x46533236
    result = []
    for _ in range(256):
        value = (value * 1664525 + 1013904223) & 0xffffffff
        result.append(value)
    return result

GEAR = gear_table()

def chunks(data):
    start, value = 0, 0
    for pos, byte in enumerate(data):
        value = ((value << 1) + GEAR[byte]) & 0xffffffff
        size = pos + 1 - start
        if size >= MIN_CHUNK and (value & MASK == 0 or size >= MAX_CHUNK):
            yield start, data[start:pos + 1]
            start, value = pos + 1, 0
    if start < len(data):
        yield start, data[start:]

def digest(data):
    return hashlib.sha256(data).hexdigest()

def public_xml(key):
    numbers = key.public_key().public_numbers()
    enc = lambda n: base64.b64encode(n.to_bytes((n.bit_length()+7)//8, 'big')).decode()
    return '<RSAKeyValue><Modulus>'+enc(numbers.n)+'</Modulus><Exponent>'+enc(numbers.e)+'</Exponent></RSAKeyValue>'

def envelope(payload, key):
    data = json.dumps(payload, ensure_ascii=False, separators=(',', ':')).encode()
    return dict(payload=base64.b64encode(data).decode(),
                signature=base64.b64encode(key.sign(data, padding.PKCS1v15(), hashes.SHA256())).decode())

def validate_release_notes(notes):
    categories = {'bug_fixes', 'improvements', 'features'}
    if not isinstance(notes, dict):
        raise ValueError('Release notes must be an object keyed by UI language')
    for language, sections in notes.items():
        if not re.fullmatch(r'[a-z]{2}', language) or not isinstance(sections, dict) or sections.keys() - categories:
            raise ValueError(f'Invalid release note sections for {language}')
        for category, entries in sections.items():
            if not isinstance(entries, list) or any(not isinstance(entry, str) or not entry.strip() for entry in entries):
                raise ValueError(f'Release notes for {language}.{category} must be a list of non-empty strings')

def publish(setup, out, key, version, family=FAMILY, next_endpoint=None, *, release_notes=None):
    if not re.fullmatch(r'(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)', version):
        raise ValueError('Use a new three-part release version')
    if release_notes is not None:
        validate_release_notes(release_notes)
    out = Path(out)
    if (out/'latest.json').exists():
        raise ValueError('Build into a new release folder; publish latest.json last')
    data = Path(setup).read_bytes()
    if not data.startswith(b'MZ') or len(data) > 1024**3:
        raise ValueError('Expected a Windows setup EXE below 1 GiB')
    (out/'chunks').mkdir(parents=True, exist_ok=True)
    entries = []
    for _, block in chunks(data):
        raw_hash = digest(block)
        packed = gzip.compress(block, compresslevel=9, mtime=0)
        target = out/'chunks'/(raw_hash+'.gz')
        if target.exists() and target.read_bytes() != packed:
            raise ValueError('Conflicting content-addressed chunk')
        target.write_bytes(packed)
        entries.append(dict(hash=raw_hash, size=len(block), packed_hash=digest(packed), packed_size=len(packed)))
    payload = dict(schema=1, family=family.lower(), version=version,
                   expires=(datetime.now(timezone.utc)+timedelta(days=30)).isoformat(),
                   size=len(data), sha256=digest(data), chunks=entries)
    if next_endpoint:
        if not next_endpoint.startswith('https://'):
            raise ValueError('The successor manifest must use HTTPS')
        payload['next_endpoint'] = next_endpoint
    if release_notes:
        payload['release_notes'] = release_notes
    (out/'latest.json').write_text(json.dumps(envelope(payload, key)), 'utf8')
    return payload

def estimate(payload, bases=()):
    found=set()
    for base in bases:
        found.update(digest(data) for _, data in chunks(Path(base).read_bytes()))
    unique={c['hash']: c for c in payload['chunks']}
    missing=[c for h,c in unique.items() if h not in found]
    size=sum(c['packed_size'] for c in missing)
    return dict(download_bytes=size, reused_bytes=sum(c['size'] for c in payload['chunks'] if c['hash'] in found),
                full_download_bytes=sum(c['packed_size'] for c in unique.values()),
                mbps_100_users_in_60_seconds=size*8*100/60/1e6,
                traffic_2000_users_gib=size*2000/1024**3)

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--setup', type=Path, required=True);p.add_argument('--output', type=Path, required=True)
    p.add_argument('--key', type=Path, required=True);p.add_argument('--version', required=True)
    p.add_argument('--base-msi', type=Path, action='append', default=[]);p.add_argument('--next-endpoint')
    p.add_argument('--release-notes', type=Path, help='UTF-8 JSON with localized bug fixes, improvements and features')
    a=p.parse_args()
    key=serialization.load_pem_private_key(a.key.read_bytes(), password=None)
    if not isinstance(key, rsa.RSAPrivateKey) or key.key_size < 3072:
        raise ValueError('Use an RSA signing key of at least 3072 bits')
    notes=None if a.release_notes is None else json.loads(a.release_notes.read_text('utf-8-sig'))
    result=publish(a.setup,a.output,key,a.version,next_endpoint=a.next_endpoint,release_notes=notes)
    measured=estimate(result,a.base_msi)
    (a.output/'size-report.json').write_text(json.dumps(measured,indent=2),'utf8')
    print(json.dumps(measured))

if __name__=='__main__':main()
