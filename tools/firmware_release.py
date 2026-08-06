#!/usr/bin/env python3
"""Prepare signed OpenSprinkler firmware release artifacts."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


MAX_RELEASES = 3
# Must match FIRMWARE_UPDATE_CATALOG_LIMIT in services/firmware_update.cpp.
MAX_MANIFEST_BYTES = 1900


def run(*args):
    subprocess.run(args, check=True)


def public_point(private_key):
    result = subprocess.run(
        ["openssl", "ec", "-in", str(private_key), "-pubout", "-conv_form", "uncompressed", "-outform", "DER"],
        check=True, capture_output=True)
    point = result.stdout[-65:]
    if len(point) != 65 or point[0] != 0x04:
        raise RuntimeError("Unable to extract an uncompressed P-256 public key")
    return point


def write_public_header(path, point):
    rows = []
    for offset in range(0, len(point), 8):
        rows.append("\t" + ", ".join(f"0x{value:02x}" for value in point[offset:offset + 8]))
    body = ",\n".join(rows)
    content = f"""#pragma once

#include <stdint.h>

// Public half of the offline OpenSprinkler firmware release key.
#define FW_UPDATE_PUBLIC_KEY_CONFIGURED 1

static const uint8_t FW_UPDATE_PUBLIC_KEY[65] = {{
{body}
}};
"""
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="ascii")


def keygen(args):
    private_key = Path(args.private_key).expanduser().resolve()
    if private_key.exists():
        raise RuntimeError(f"Refusing to overwrite private key: {private_key}")
    private_key.parent.mkdir(parents=True, exist_ok=True)
    run("openssl", "ecparam", "-name", "prime256v1", "-genkey", "-noout", "-out", str(private_key))
    os.chmod(private_key, 0o600)
    write_public_header(Path(args.public_header), public_point(private_key))
    print(f"Private key created at {private_key}")
    print("Back it up offline. Commit only the generated public-key header.")


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def dotted_version(value):
    text = str(value)
    if len(text) != 3 or not text.isdigit():
        raise ValueError("--version must be a three-digit firmware version such as 221")
    return ".".join(text)


def artifact(source, destination, url_path, min_flash=None):
    if destination.exists():
        if sha256(source) != sha256(destination):
            raise RuntimeError(f"Refusing to replace immutable artifact: {destination}")
    else:
        shutil.copy2(source, destination)
    result = {
        "file": url_path,
        "size": destination.stat().st_size,
        "sha256": sha256(destination),
    }
    if min_flash:
        result["min_flash"] = min_flash
    return result


def compact_release(release):
    targets = {}
    for target, source in release["targets"].items():
        result = {
            "file": source["file"],
            "size": source["size"],
            "sha256": source["sha256"],
        }
        if source.get("min_flash"):
            result["min_flash"] = source["min_flash"]
        targets[target] = result
    return {
        "id": release["id"],
        "version": release["version"],
        "build": release["build"],
        "targets": targets,
    }


def prepare(args):
    private_key = Path(args.private_key).expanduser().resolve()
    if not private_key.is_file():
        raise RuntimeError(f"Private signing key not found: {private_key}")
    output = Path(args.output_dir).resolve()
    output.mkdir(parents=True, exist_ok=True)
    release_id = args.release_id or f"{dotted_version(args.version)}-{args.build}"
    if len(release_id) >= 20:
        raise RuntimeError("Release id exceeds the firmware catalog limit")
    if args.build < 0 or args.build > 65535 or args.sequence <= 0:
        raise RuntimeError("Build must fit uint16_t and sequence must be positive")
    release_dir = output / "releases" / release_id
    release_dir.mkdir(parents=True, exist_ok=True)

    esp8266_name = f"opensprinkler-{release_id}-esp8266.bin"
    esp32_name = f"opensprinkler-{release_id}-esp32c6.bin32"
    esp8266_destination = release_dir / esp8266_name
    esp32_destination = release_dir / esp32_name

    release = {
        "id": release_id,
        "version": args.version,
        "build": args.build,
        "targets": {
            "os3-esp8266": artifact(Path(args.esp8266), esp8266_destination,
                f"/v1/releases/{release_id}/{esp8266_name}"),
            "os4-esp32c6": artifact(Path(args.esp32c6), esp32_destination,
                f"/v1/releases/{release_id}/{esp32_name}", 8 * 1024 * 1024),
        },
    }

    manifest_path = output / "manifest.json"
    releases = []
    if manifest_path.exists():
        previous = json.loads(manifest_path.read_text(encoding="utf-8"))
        if args.sequence <= int(previous.get("sequence", 0)):
            raise RuntimeError("--sequence must be greater than the current catalog sequence")
        releases = [compact_release(item) for item in previous.get("releases", [])
                    if item.get("id") != release_id]
    catalog = {
        "schema": 1,
        "sequence": args.sequence,
        "releases": [release] + releases[:MAX_RELEASES - 1],
    }
    encoded = (json.dumps(catalog, separators=(",", ":"), ensure_ascii=True) + "\n").encode("ascii")
    if len(encoded) > args.max_manifest_bytes:
        raise RuntimeError(f"Manifest is {len(encoded)} bytes; firmware limit is {args.max_manifest_bytes}")

    with tempfile.TemporaryDirectory() as directory:
        temporary = Path(directory)
        unsigned = temporary / "manifest.json"
        signature = temporary / "manifest.sig.der"
        public_key = temporary / "public.pem"
        unsigned.write_bytes(encoded)
        run("openssl", "dgst", "-sha256", "-sign", str(private_key), "-out", str(signature), str(unsigned))
        run("openssl", "ec", "-in", str(private_key), "-pubout", "-out", str(public_key))
        run("openssl", "dgst", "-sha256", "-verify", str(public_key), "-signature", str(signature), str(unsigned))
        manifest_tmp = output / "manifest.json.tmp"
        signature_tmp = output / "manifest.sig.tmp"
        manifest_tmp.write_bytes(encoded)
        signature_tmp.write_text(signature.read_bytes().hex() + "\n", encoding="ascii")
        manifest_tmp.replace(manifest_path)
        signature_tmp.replace(output / "manifest.sig")

    print(f"Prepared {release_id} in {output}")
    print("Upload the releases directory first, then publish manifest.json and manifest.sig last.")


def parser():
    result = argparse.ArgumentParser(description=__doc__)
    commands = result.add_subparsers(dest="command", required=True)

    key = commands.add_parser("keygen", help="create the offline P-256 release key")
    key.add_argument("--private-key", required=True)
    key.add_argument("--public-header", default="services/firmware_update_public_key.h")
    key.set_defaults(func=keygen)

    release = commands.add_parser("prepare", help="copy, hash, catalog, and sign a firmware release")
    release.add_argument("--private-key", required=True)
    release.add_argument("--output-dir", required=True, help="local v1 publication directory")
    release.add_argument("--esp8266", required=True)
    release.add_argument("--esp32c6", required=True)
    release.add_argument("--version", required=True, type=int, help="for example 221")
    release.add_argument("--build", required=True, type=int, help="for example 6")
    release.add_argument("--sequence", required=True, type=int, help="monotonically increasing catalog number")
    release.add_argument("--release-id")
    release.add_argument("--max-manifest-bytes", type=int, default=MAX_MANIFEST_BYTES,
                         help="catalog size limit; must not exceed the firmware limit")
    release.set_defaults(func=prepare)
    return result


if __name__ == "__main__":
    arguments = parser().parse_args()
    try:
        arguments.func(arguments)
    except (OSError, RuntimeError, ValueError, subprocess.CalledProcessError) as error:
        raise SystemExit(f"error: {error}")
