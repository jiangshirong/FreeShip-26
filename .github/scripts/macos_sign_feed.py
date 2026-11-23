"""Create a channel-bound FreeShip macOS feed with the local release identity.

The private key is used only on the publisher's machine. The output is public
JSON; it contains the payload and signature, never the private key.
"""
import argparse
import hashlib
import json
import re
import sys
from datetime import datetime, timedelta, timezone
from pathlib import Path
from urllib.parse import urlsplit

from cryptography.hazmat.primitives import serialization

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "FreeShip-Qt/tools"))
from update_release import envelope, public_xml, validate_release_notes


def require_https(value):
    url = urlsplit(value)
    if url.scheme != "https" or not url.hostname or url.username or url.password or url.fragment or url.query:
        raise ValueError("Expected a public HTTPS URL without credentials, query or fragment")
    return value


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--channel", required=True, type=require_https, help="Full endpoint ending in /<arch>/latest.json")
    parser.add_argument("--arch", choices=("arm64", "x86_64"), required=True)
    parser.add_argument("--version", required=True)
    parser.add_argument("--minimum-macos", default="15.0")
    parser.add_argument("--key", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--days", type=int, default=30)
    parser.add_argument("--dmg", type=Path)
    parser.add_argument("--url", type=require_https)
    parser.add_argument("--verification", type=Path, help="Passing macOS package verification.json")
    parser.add_argument("--release-notes", type=Path)
    args = parser.parse_args()
    if not args.channel.endswith(f"/{args.arch}/latest.json"):
        parser.error("Channel must include the selected architecture")
    if not 1 <= args.days <= 90 or not re.fullmatch(r"(?:0|[1-9][0-9]{0,7})(?:\.(?:0|[1-9][0-9]{0,7})){2}", args.version):
        parser.error("Invalid expiry or release version")
    if not re.fullmatch(r"[0-9]{1,3}\.[0-9]{1,3}(\.[0-9]{1,3})?", args.minimum_macos):
        parser.error("Invalid minimum macOS version")
    config = json.loads((ROOT / "FreeShip-Qt/tools/updates/channel.json").read_bytes())
    key = serialization.load_pem_private_key(args.key.read_bytes(), None)
    if public_xml(key) != config["public_key_xml"] or key.key_size < 3072:
        raise ValueError("Signing key does not match the compiled FreeShip public identity")
    payload = {
        "schema": "fs26-macos-dmg-v1", "channel": args.channel, "family": config["family"], "arch": args.arch,
        "version": args.version, "minimum_macos": args.minimum_macos,
        "expires": (datetime.now(timezone.utc) + timedelta(days=args.days)).strftime("%Y-%m-%dT%H:%M:%SZ"),
    }
    if args.dmg or args.url or args.verification:
        if not (args.dmg and args.url and args.verification):
            parser.error("DMG publication requires --dmg, --url and --verification together")
        verification = json.loads(args.verification.read_bytes())
        with args.dmg.open("rb") as source:
            digest = hashlib.file_digest(source, "sha256").hexdigest()
        size = args.dmg.stat().st_size
        if not 0 < size <= 1024**3 or any(verification.get(k) != v for k, v in {
            "status": "pass", "version": args.version, "architecture": args.arch, "deployment_target": args.minimum_macos,
            "update_channel_url": args.channel, "package": args.dmg.name, "sha256": digest, "package_bytes": size,
        }.items()):
            raise ValueError("DMG does not match the passing build report for this channel")
        payload["package"] = {"url": args.url, "sha256": digest, "size": size}
    if args.release_notes:
        notes = json.loads(args.release_notes.read_bytes())
        validate_release_notes(notes)
        payload["release_notes"] = notes
    encoded = json.dumps(envelope(payload, key), indent=2) + "\n"
    if len(encoded.encode()) > 1024**2:
        raise ValueError("Manifest exceeds updater size limit")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(encoded, encoding="utf-8")
    print(f"Signed {args.arch} {args.version} update metadata, valid until {payload['expires']}")


if __name__ == "__main__":
    main()
