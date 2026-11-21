"""Build the private Flatpak index with the notes for the matching signed Windows release."""

import argparse, base64, json, os, subprocess, urllib.request
import xml.etree.ElementTree as ET
from pathlib import Path
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.asymmetric import rsa, padding

p = argparse.ArgumentParser(description=__doc__)
p.add_argument("webroot", type=Path)
p.add_argument("channel", type=Path)
p.add_argument("--version", required=True)
p.add_argument("--revision", type=int, required=True)
p.add_argument(
    "--windows-envelope", type=Path, help="saved signed Windows manifest for this version"
)
a = p.parse_args()
app = "io.github.jiangshirong.FreeShip26"
ref = f"app/{app}/x86_64/update-test"
repo = a.webroot / "repo"
commit = subprocess.check_output(
    ["ostree", "--repo=" + str(repo), "rev-parse", ref], text=True
).strip()
build = json.loads(
    subprocess.check_output(
        [
            "ostree",
            "--repo=" + str(repo),
            "cat",
            commit,
            "/files/lib/freeship26/build-manifest.json",
        ]
    )
)
if (build["release_version"], build["flatpak_revision"]) != (a.version, a.revision):
    raise ValueError("Flatpak version mismatch")
cfg = json.loads(a.channel.read_bytes())
if a.windows_envelope:
    envelope = json.loads(a.windows_envelope.read_bytes())
else:
    with urllib.request.urlopen(cfg["endpoint"], timeout=15) as resp:
        envelope = json.load(resp)
payload = base64.b64decode(envelope["payload"], validate=True)
xml = ET.fromstring(cfg["public_key_xml"])


def number(name):
    return int.from_bytes(base64.b64decode(xml.findtext(name), validate=True), "big")


rsa.RSAPublicNumbers(number("Exponent"), number("Modulus")).public_key().verify(
    base64.b64decode(envelope["signature"], validate=True),
    payload,
    padding.PKCS1v15(),
    hashes.SHA256(),
)
win = json.loads(payload)
if (win["family"], win["version"]) != (cfg["family"], a.version):
    raise ValueError("No matching Windows release notes")
index = dict(
    app=app,
    arch="x86_64",
    branch="update-test",
    version=a.version,
    revision=a.revision,
    commit=commit,
    release_notes=win["release_notes"],
)
tmp = a.webroot / "latest.json.tmp"
tmp.write_text(json.dumps(index, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
os.replace(tmp, a.webroot / "latest.json")
print(commit, win["version"], ",".join(index["release_notes"]))
