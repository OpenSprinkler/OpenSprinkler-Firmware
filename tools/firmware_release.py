#!/usr/bin/env python3
"""Build, sign, publish, and verify OpenSprinkler firmware releases."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import tempfile
import urllib.error
import urllib.parse
import urllib.request


REPO_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_PUBLIC_HEADER = REPO_ROOT / "services" / "firmware_update_public_key.h"
DEFAULT_BASE_URL = "https://firmware.opensprinkler.com"
MAX_DESCRIPTOR_BYTES = 1024


def run(command, *, dry_run=False, capture_output=False):
    print("+", shlex.join(str(value) for value in command))
    if dry_run:
        return None
    return subprocess.run(
        [str(value) for value in command], check=True,
        capture_output=capture_output)


def public_point(private_key):
    result = subprocess.run(
        ["openssl", "ec", "-in", str(private_key), "-pubout",
         "-conv_form", "uncompressed", "-outform", "DER"],
        check=True, capture_output=True)
    point = result.stdout[-65:]
    if len(point) != 65 or point[0] != 0x04:
        raise RuntimeError("Unable to extract an uncompressed P-256 public key")
    return point


def public_point_from_header(path):
    path = Path(path).resolve()
    if not path.is_file():
        raise RuntimeError(f"Firmware public-key header not found: {path}")
    values = re.findall(r"0x([0-9a-fA-F]{2})", path.read_text(encoding="ascii"))
    point = bytes(int(value, 16) for value in values)
    if len(point) != 65 or point[0] != 0x04:
        raise RuntimeError(f"Expected one 65-byte P-256 public key in {path}")
    return point


def require_matching_key(private_key, public_header):
    private_key = Path(private_key).expanduser().resolve()
    if not private_key.is_file():
        raise RuntimeError(f"Private signing key not found: {private_key}")
    if public_point(private_key) != public_point_from_header(public_header):
        raise RuntimeError("Private signing key does not match the public key embedded in firmware")
    return private_key


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
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="ascii")


def keygen(args):
    private_key = Path(args.private_key).expanduser().resolve()
    if private_key.exists():
        raise RuntimeError(f"Refusing to overwrite private key: {private_key}")
    private_key.parent.mkdir(parents=True, exist_ok=True)
    run(["openssl", "ecparam", "-name", "prime256v1", "-genkey", "-noout",
         "-out", private_key])
    os.chmod(private_key, 0o600)
    write_public_header(args.public_header, public_point(private_key))
    print(f"Private key created at {private_key}")
    print("Back it up offline. Commit only the generated public-key header.")


def sha256(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def dotted_version(value):
    text = str(value)
    if len(text) != 3 or not text.isdigit():
        raise ValueError("Firmware version must be three digits, such as 221")
    return ".".join(text)


def firmware_version(defines_path=REPO_ROOT / "defines.h"):
    source = Path(defines_path).read_text(encoding="utf-8")
    version_match = re.search(r"^\s*#define\s+OS_FW_VERSION\s+(\d+)", source, re.MULTILINE)
    build_match = re.search(r"^\s*#define\s+OS_FW_MINOR\s+(\d+)", source, re.MULTILINE)
    if not version_match or not build_match:
        raise RuntimeError(f"Unable to read firmware version from {defines_path}")
    return int(version_match.group(1)), int(build_match.group(1))


def validate_image(path, target):
    path = Path(path).resolve()
    if not path.is_file() or path.stat().st_size < 1024:
        raise RuntimeError(f"Firmware artifact is missing or too small: {path}")
    header = path.read_bytes()[:16]
    if len(header) != 16 or header[0] != 0xE9 or not 1 <= header[1] <= 16:
        raise RuntimeError(f"Firmware artifact has an invalid image header: {path}")
    chip_id = header[12] | (header[13] << 8)
    if target == "os4-esp32c6":
        valid = chip_id == 0x000D
    else:
        entry = int.from_bytes(header[4:8], "little")
        valid = chip_id != 0x000D and 0x40000000 <= entry < 0x40400000
    if not valid:
        raise RuntimeError(f"Firmware artifact does not match target {target}: {path}")
    return path


def artifact(source, destination, url_path, min_flash=None):
    source = Path(source).resolve()
    destination = Path(destination)
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


def public_key_der(point):
    # SubjectPublicKeyInfo for id-ecPublicKey + prime256v1 + uncompressed point.
    prefix = bytes.fromhex("3059301306072a8648ce3d020106082a8648ce3d030107034200")
    return prefix + point


def verify_signature(payload, signature_text, point):
    try:
        signature = bytes.fromhex(signature_text.decode("ascii").strip())
    except (UnicodeDecodeError, ValueError) as error:
        raise RuntimeError("Release signature is not valid hexadecimal DER") from error
    with tempfile.TemporaryDirectory() as directory:
        temporary = Path(directory)
        payload_path = temporary / "release.json"
        signature_path = temporary / "release.sig.der"
        public_der = temporary / "public.der"
        public_pem = temporary / "public.pem"
        payload_path.write_bytes(payload)
        signature_path.write_bytes(signature)
        public_der.write_bytes(public_key_der(point))
        run(["openssl", "pkey", "-pubin", "-inform", "DER", "-in", public_der,
             "-out", public_pem])
        run(["openssl", "dgst", "-sha256", "-verify", public_pem,
             "-signature", signature_path, payload_path])


def catalog_root(base_url):
    base = base_url.rstrip("/")
    return base if base.endswith("/v1") else base + "/v1"


def origin_url(base_url):
    parsed = urllib.parse.urlsplit(base_url)
    if not parsed.scheme or not parsed.netloc:
        raise ValueError(f"Invalid firmware base URL: {base_url}")
    return urllib.parse.urlunsplit((parsed.scheme, parsed.netloc, "", "", ""))


def fetch(url, *, timeout=30):
    request = urllib.request.Request(url, headers={
        "Cache-Control": "no-cache",
        "User-Agent": "OpenSprinkler-Firmware-Release/1",
    })
    with urllib.request.urlopen(request, timeout=timeout) as response:
        return response.read(), response.headers


def require_cors(headers, url):
    if headers.get("Access-Control-Allow-Origin") != "*":
        raise RuntimeError(f"CORS is not enabled for firmware download: {url}")


def parse_catalog(payload, source):
    try:
        catalog = json.loads(payload.decode("ascii"))
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise RuntimeError(f"Unable to read release catalog: {source}") from error
    if not isinstance(catalog, dict) or not isinstance(catalog.get("releases"), list):
        raise RuntimeError(f"Unsupported release catalog format: {source}")
    return catalog


def load_catalog(path):
    path = Path(path)
    try:
        catalog = parse_catalog(path.read_bytes(), path)
    except OSError as error:
        raise RuntimeError(f"Unable to read release catalog: {path}") from error
    if catalog.get("schema") != 2:
        raise RuntimeError(f"Unsupported release catalog format: {path}")
    return catalog


def release_paths(release_id):
    base = f"/v1/releases/{release_id}"
    return base + "/release.json", base + "/release.sig"


def validate_catalog_entry(entry):
    if not isinstance(entry, dict):
        raise RuntimeError(f"Invalid release catalog entry: {entry!r}")
    release_id = entry.get("id")
    descriptor, signature = release_paths(release_id or "")
    targets = entry.get("targets")
    if (not isinstance(release_id, str) or
            not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._-]{0,30}", release_id) or
            entry.get("descriptor") != descriptor or entry.get("signature") != signature or
            not isinstance(targets, list) or not targets or
            len(targets) != len(set(targets)) or
            not all(target in ("os3-esp8266", "os4-esp32c6") for target in targets)):
        raise RuntimeError(f"Invalid release catalog entry: {release_id!r}")
    return release_id


def output_path_for_url(output, path):
    if not path.startswith("/v1/") or ".." in path:
        raise RuntimeError(f"Unsafe release path: {path}")
    return Path(output) / path[len("/v1/"):]


def sync_remote_catalog(base_url, output, point):
    output = Path(output).resolve()
    root = catalog_root(base_url)
    try:
        manifest, manifest_headers = fetch(root + "/manifest.json")
    except urllib.error.HTTPError as error:
        if error.code == 404:
            print("No remote release catalog exists; starting a new catalog.")
            return
        raise
    require_cors(manifest_headers, root + "/manifest.json")
    output.mkdir(parents=True, exist_ok=True)
    remote = parse_catalog(manifest, root + "/manifest.json")
    if remote.get("schema") == 1:
        fresh = {"schema": 2, "releases": []}
        (output / "manifest.json").write_text(
            json.dumps(fresh, separators=(",", ":")) + "\n", encoding="ascii")
        print("Ignoring the development-only schema-1 catalog and starting a schema-2 catalog.")
        return
    if remote.get("schema") != 2:
        raise RuntimeError("Unsupported remote release catalog schema")
    for entry in remote["releases"]:
        validate_catalog_entry(entry)
        descriptor_url = origin_url(base_url) + entry["descriptor"]
        signature_url = origin_url(base_url) + entry["signature"]
        descriptor, descriptor_headers = fetch(descriptor_url)
        signature, signature_headers = fetch(signature_url)
        require_cors(descriptor_headers, descriptor_url)
        require_cors(signature_headers, signature_url)
        verify_signature(descriptor, signature, point)
        if len(descriptor) > MAX_DESCRIPTOR_BYTES:
            raise RuntimeError(f"Signed descriptor is too large for {entry['id']}")
        descriptor_json = json.loads(descriptor.decode("ascii"))
        if descriptor_json.get("schema") != 1 or descriptor_json.get("id") != entry["id"]:
            raise RuntimeError(f"Invalid signed descriptor for {entry['id']}")
        descriptor_path = output_path_for_url(output, entry["descriptor"])
        signature_path = output_path_for_url(output, entry["signature"])
        descriptor_path.parent.mkdir(parents=True, exist_ok=True)
        descriptor_path.write_bytes(descriptor)
        signature_path.write_bytes(signature)
    (output / "manifest.json").write_bytes(manifest)
    print(f"Synchronized {len(remote['releases'])} signed releases from {root}")


def prepare(args):
    private_key = require_matching_key(args.private_key, args.public_header)
    output = Path(args.output_dir).resolve()
    output.mkdir(parents=True, exist_ok=True)
    detected_version, detected_build = firmware_version()
    version = args.version if args.version is not None else detected_version
    build = args.build if args.build is not None else detected_build
    release_id = args.release_id or f"{dotted_version(version)}-{build}"
    if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._-]{0,30}", release_id):
        raise RuntimeError(
            "Release id must be 1-31 letters, digits, dots, underscores, or hyphens")
    if build < 0 or build > 65535:
        raise RuntimeError("Build must fit uint16_t")

    manifest_path = output / "manifest.json"
    releases = []
    if manifest_path.exists():
        previous = load_catalog(manifest_path)
        if any(item.get("id") == release_id for item in previous.get("releases", [])):
            raise RuntimeError(
                f"Release id {release_id} already exists; publish the prepared catalog or use a new id")
        releases = previous.get("releases", [])

    esp8266_source = validate_image(args.esp8266, "os3-esp8266")
    esp32_source = validate_image(args.esp32c6, "os4-esp32c6")
    release_dir = output / "releases" / release_id
    release_dir.mkdir(parents=True, exist_ok=True)
    esp8266_name = f"opensprinkler-{release_id}-esp8266.bin"
    esp32_name = f"opensprinkler-{release_id}-esp32c6.bin32"

    descriptor = {
        "schema": 1,
        "id": release_id,
        "version": version,
        "build": build,
        "targets": {
            "os3-esp8266": artifact(esp8266_source, release_dir / esp8266_name,
                f"/v1/releases/{release_id}/{esp8266_name}"),
            "os4-esp32c6": artifact(esp32_source, release_dir / esp32_name,
                f"/v1/releases/{release_id}/{esp32_name}", 8 * 1024 * 1024),
        },
    }
    descriptor_bytes = (
        json.dumps(descriptor, separators=(",", ":"), ensure_ascii=True) + "\n").encode("ascii")
    if len(descriptor_bytes) > MAX_DESCRIPTOR_BYTES:
        raise RuntimeError(
            f"Release descriptor is {len(descriptor_bytes)} bytes; limit is {MAX_DESCRIPTOR_BYTES}")
    descriptor_path = release_dir / "release.json"
    signature_path = release_dir / "release.sig"
    if descriptor_path.exists():
        if descriptor_path.read_bytes() != descriptor_bytes:
            raise RuntimeError(f"Refusing to replace immutable release descriptor: {descriptor_path}")
        if not signature_path.is_file():
            raise RuntimeError(f"Release signature is missing: {signature_path}")
        verify_signature(descriptor_bytes, signature_path.read_bytes(), public_point(private_key))
    else:
        if signature_path.exists():
            raise RuntimeError(f"Release descriptor is missing: {descriptor_path}")
        descriptor_path.write_bytes(descriptor_bytes)
        with tempfile.TemporaryDirectory() as directory:
            signature_der = Path(directory) / "release.sig.der"
            run(["openssl", "dgst", "-sha256", "-sign", private_key,
                 "-out", signature_der, descriptor_path])
            signature_path.write_text(signature_der.read_bytes().hex() + "\n", encoding="ascii")
        verify_signature(descriptor_bytes, signature_path.read_bytes(), public_point(private_key))

    descriptor_url, signature_url = release_paths(release_id)
    index_entry = {
        "id": release_id,
        "version": version,
        "build": build,
        "targets": list(descriptor["targets"].keys()),
        "descriptor": descriptor_url,
        "signature": signature_url,
    }
    catalog = {
        "schema": 2,
        "releases": [index_entry] + releases,
    }
    encoded = (json.dumps(catalog, separators=(",", ":"), ensure_ascii=True) + "\n").encode("ascii")
    manifest_tmp = output / "manifest.json.tmp"
    manifest_tmp.write_bytes(encoded)
    manifest_tmp.replace(manifest_path)

    print(f"Prepared signed release {release_id} in {output}")
    print("Publish release files first; publish manifest.json last.")
    return release_id


def remote_destination(value):
    if ":" not in value:
        raise ValueError("Publish destination must be in host:/absolute/path form")
    host, path = value.split(":", 1)
    if not host or not path.startswith("/"):
        raise ValueError("Publish destination must be in host:/absolute/path form")
    return host, path.rstrip("/")


def verify_publication(base_url, public_header, expected_dir=None):
    point = public_point_from_header(public_header)
    root = catalog_root(base_url)
    manifest, manifest_headers = fetch(root + "/manifest.json")
    require_cors(manifest_headers, root + "/manifest.json")
    catalog = parse_catalog(manifest, root + "/manifest.json")
    if catalog.get("schema") != 2:
        raise RuntimeError("Published catalog is not schema 2")
    if expected_dir:
        expected = Path(expected_dir)
        if manifest != (expected / "manifest.json").read_bytes():
            raise RuntimeError("Published manifest does not match the local release catalog")
    origin = origin_url(base_url)
    checked_releases = 0
    checked = 0
    for entry in catalog.get("releases", []):
        validate_catalog_entry(entry)
        descriptor_url = origin + entry["descriptor"]
        signature_url = origin + entry["signature"]
        descriptor, descriptor_headers = fetch(descriptor_url)
        signature, signature_headers = fetch(signature_url)
        require_cors(descriptor_headers, descriptor_url)
        require_cors(signature_headers, signature_url)
        verify_signature(descriptor, signature, point)
        release = json.loads(descriptor.decode("ascii"))
        if release.get("schema") != 1 or release.get("id") != entry["id"]:
            raise RuntimeError(f"Published release descriptor is invalid: {entry['id']}")
        if set(entry["targets"]) != set(release.get("targets", {})):
            raise RuntimeError(f"Catalog targets do not match signed release: {entry['id']}")
        if expected_dir:
            local_descriptor = output_path_for_url(expected, entry["descriptor"])
            local_signature = output_path_for_url(expected, entry["signature"])
            if descriptor != local_descriptor.read_bytes() or signature != local_signature.read_bytes():
                raise RuntimeError(f"Published signed release differs locally: {entry['id']}")
        for target, item in release.get("targets", {}).items():
            path = item.get("file", "")
            if not path.startswith("/v1/releases/") or ".." in path:
                raise RuntimeError(f"Unsafe artifact path for {target}: {path}")
            url = origin + path
            request = urllib.request.Request(url, headers={
                "Cache-Control": "no-cache",
                "User-Agent": "OpenSprinkler-Firmware-Release/1",
            })
            digest = hashlib.sha256()
            size = 0
            with urllib.request.urlopen(request, timeout=60) as response:
                require_cors(response.headers, url)
                while True:
                    block = response.read(1024 * 1024)
                    if not block:
                        break
                    size += len(block)
                    digest.update(block)
            if size != int(item.get("size", 0)) or digest.hexdigest() != item.get("sha256"):
                raise RuntimeError(f"Published artifact failed verification: {url}")
            checked += 1
        checked_releases += 1
    print(f"Verified {checked_releases} signed releases and {checked} published artifacts.")


def publish(output_dir, destination, base_url, public_header, *, dry_run=False,
            allow_prune=False):
    output = Path(output_dir).resolve()
    manifest_path = output / "manifest.json"
    if not manifest_path.is_file():
        raise RuntimeError(f"Prepared catalog is missing from {output}")
    point = public_point_from_header(public_header)
    catalog = load_catalog(manifest_path)
    for entry in catalog["releases"]:
        validate_catalog_entry(entry)
        descriptor = output_path_for_url(output, entry["descriptor"])
        signature = output_path_for_url(output, entry["signature"])
        if not descriptor.is_file() or not signature.is_file():
            raise RuntimeError(f"Signed release files are missing for {entry['id']}")
        verify_signature(descriptor.read_bytes(), signature.read_bytes(), point)

    remote_manifest = None
    try:
        remote_manifest, remote_manifest_headers = fetch(catalog_root(base_url) + "/manifest.json")
    except urllib.error.HTTPError as error:
        if error.code != 404:
            raise
    if remote_manifest is not None:
        require_cors(remote_manifest_headers, catalog_root(base_url) + "/manifest.json")
        remote_catalog = parse_catalog(remote_manifest, catalog_root(base_url) + "/manifest.json")
        if remote_catalog.get("schema") == 2:
            local_entries = {entry["id"]: entry for entry in catalog["releases"]}
            remote_ids = {validate_catalog_entry(entry) for entry in remote_catalog["releases"]}
            missing_ids = sorted(remote_ids - set(local_entries))
            if missing_ids and not allow_prune:
                raise RuntimeError(
                    "Local catalog would remove published release(s): " + ", ".join(missing_ids) +
                    "; pass --allow-prune to confirm")
            for remote_entry in remote_catalog["releases"]:
                release_id = validate_catalog_entry(remote_entry)
                if release_id not in local_entries:
                    continue
                remote_descriptor, _ = fetch(origin_url(base_url) + remote_entry["descriptor"])
                local_descriptor = output_path_for_url(output, local_entries[release_id]["descriptor"])
                if remote_descriptor != local_descriptor.read_bytes():
                    raise RuntimeError(f"Refusing to replace immutable release: {release_id}")
        elif remote_catalog.get("schema") == 1:
            remote_ids = {entry.get("id") for entry in remote_catalog.get("releases", [])}
            local_ids = {entry.get("id") for entry in catalog.get("releases", [])}
            conflicts = sorted((remote_ids & local_ids) - {None})
            if conflicts:
                raise RuntimeError(
                    "Refusing to reuse legacy release id(s): " + ", ".join(conflicts))
        else:
            raise RuntimeError("Unsupported remote release catalog schema")

    host, remote_path = remote_destination(destination)
    releases = output / "releases"
    if not releases.is_dir():
        raise RuntimeError(f"Release artifact directory is missing: {releases}")
    run(["ssh", host, f"mkdir -p {shlex.quote(remote_path + '/releases')}"] , dry_run=dry_run)
    for entry in catalog["releases"]:
        release_id = entry["id"]
        release_dir = releases / release_id
        if not release_dir.is_dir():
            raise RuntimeError(f"Release directory is missing: {release_dir}")
        run(["ssh", host,
             f"mkdir -p {shlex.quote(remote_path + '/releases/' + release_id)}"],
            dry_run=dry_run)
        run(["rsync", "-az", "--checksum", "--delay-updates", "--chmod=D755,F644",
             str(release_dir) + "/",
             f"{host}:{remote_path}/releases/{release_id}/"], dry_run=dry_run)
    run(["rsync", "-az", "--checksum", manifest_path,
         f"{host}:{remote_path}/.manifest.json.new"], dry_run=dry_run)
    activate = (f"mv {shlex.quote(remote_path + '/.manifest.json.new')} "
                f"{shlex.quote(remote_path + '/manifest.json')}")
    run(["ssh", host, activate], dry_run=dry_run)
    if dry_run:
        print("Dry run complete; no remote files were changed.")
    else:
        verify_publication(base_url, public_header, output)


def resolve_pio(value=None):
    if value:
        return value
    configured = os.environ.get("PLATFORMIO_CMD")
    if configured:
        return configured
    found = shutil.which("pio")
    if found:
        return found
    fallback = Path.home() / ".platformio" / "penv" / "bin" / "pio"
    if fallback.is_file():
        return str(fallback)
    raise RuntimeError("PlatformIO command not found; use --pio or PLATFORMIO_CMD")


def build_firmware(pio=None):
    command = resolve_pio(pio)
    run([command, "run", "-e", "os3x_esp8266", "-e", "os4_esp32c6"])
    esp8266 = REPO_ROOT / ".pio" / "build" / "os3x_esp8266" / "firmware.bin"
    esp32c6 = REPO_ROOT / ".pio" / "build" / "os4_esp32c6" / "firmware.bin32"
    validate_image(esp8266, "os3-esp8266")
    validate_image(esp32c6, "os4-esp32c6")
    return esp8266, esp32c6


def release(args):
    private_key = require_matching_key(args.private_key, args.public_header)
    point = public_point(private_key)
    if not args.no_sync:
        sync_remote_catalog(args.base_url, args.output_dir, point)
    if args.skip_build:
        if not args.esp8266 or not args.esp32c6:
            raise RuntimeError("--skip-build requires --esp8266 and --esp32c6")
        esp8266, esp32c6 = args.esp8266, args.esp32c6
    else:
        detected_version, detected_build = firmware_version()
        if args.version is not None and args.version != detected_version:
            raise RuntimeError("--version does not match OS_FW_VERSION in the firmware being built")
        if args.build is not None and args.build != detected_build:
            raise RuntimeError("--build does not match OS_FW_MINOR in the firmware being built")
        esp8266, esp32c6 = build_firmware(args.pio)
    prepare_args = argparse.Namespace(
        private_key=private_key,
        public_header=args.public_header,
        output_dir=args.output_dir,
        esp8266=esp8266,
        esp32c6=esp32c6,
        version=args.version,
        build=args.build,
        release_id=args.release_id,
    )
    prepare(prepare_args)
    if args.destination:
        publish(args.output_dir, args.destination, args.base_url,
                args.public_header, dry_run=args.dry_run, allow_prune=args.allow_prune)
    elif args.dry_run:
        raise RuntimeError("--dry-run requires --destination")


def publish_command(args):
    publish(args.output_dir, args.destination, args.base_url,
            args.public_header, dry_run=args.dry_run, allow_prune=args.allow_prune)


def verify_command(args):
    verify_publication(args.base_url, args.public_header, args.expected_dir)


def add_public_header_argument(command):
    command.add_argument("--public-header", default=str(DEFAULT_PUBLIC_HEADER),
                         help="firmware header containing the embedded release public key")


def add_prepare_arguments(command):
    command.add_argument("--private-key", required=True)
    add_public_header_argument(command)
    command.add_argument("--output-dir", required=True, help="local v1 publication directory")
    command.add_argument("--esp8266", required=True)
    command.add_argument("--esp32c6", required=True)
    command.add_argument("--version", type=int, help="defaults to OS_FW_VERSION in defines.h")
    command.add_argument("--build", type=int, help="defaults to OS_FW_MINOR in defines.h")
    command.add_argument("--release-id")


def parser():
    result = argparse.ArgumentParser(description=__doc__)
    commands = result.add_subparsers(dest="command", required=True)

    key = commands.add_parser("keygen", help="create the offline P-256 release key")
    key.add_argument("--private-key", required=True)
    key.add_argument("--public-header", default=str(DEFAULT_PUBLIC_HEADER))
    key.set_defaults(func=keygen)

    prepare_command = commands.add_parser("prepare", help="copy, hash, catalog, and sign a release")
    add_prepare_arguments(prepare_command)
    prepare_command.set_defaults(func=prepare)

    release_command = commands.add_parser(
        "release", help="sync catalog, build both targets, prepare, and optionally publish")
    release_command.add_argument("--private-key", required=True)
    add_public_header_argument(release_command)
    release_command.add_argument("--output-dir", required=True)
    release_command.add_argument("--base-url", default=DEFAULT_BASE_URL)
    release_command.add_argument("--destination", help="optional host:/absolute/path publish target")
    release_command.add_argument("--dry-run", action="store_true",
                                 help="prepare locally but print remote writes without executing them")
    release_command.add_argument("--no-sync", action="store_true",
                                 help="do not import the current public catalog and signed releases")
    release_command.add_argument("--skip-build", action="store_true")
    release_command.add_argument("--pio")
    release_command.add_argument("--esp8266")
    release_command.add_argument("--esp32c6")
    release_command.add_argument("--version", type=int)
    release_command.add_argument("--build", type=int)
    release_command.add_argument("--release-id")
    release_command.add_argument("--allow-prune", action="store_true",
                                 help="allow the publication index to omit existing releases")
    release_command.set_defaults(func=release)

    publish_parser = commands.add_parser("publish", help="publish a prepared release tree")
    publish_parser.add_argument("--output-dir", required=True)
    publish_parser.add_argument("--destination", required=True)
    publish_parser.add_argument("--base-url", default=DEFAULT_BASE_URL)
    add_public_header_argument(publish_parser)
    publish_parser.add_argument("--dry-run", action="store_true")
    publish_parser.add_argument("--allow-prune", action="store_true",
                                help="allow the publication index to omit existing releases")
    publish_parser.set_defaults(func=publish_command)

    verify_parser = commands.add_parser("verify", help="verify the public catalog and artifacts")
    verify_parser.add_argument("--base-url", default=DEFAULT_BASE_URL)
    add_public_header_argument(verify_parser)
    verify_parser.add_argument("--expected-dir")
    verify_parser.set_defaults(func=verify_command)
    return result


if __name__ == "__main__":
    arguments = parser().parse_args()
    try:
        arguments.func(arguments)
    except (OSError, RuntimeError, ValueError, json.JSONDecodeError,
            urllib.error.URLError, subprocess.CalledProcessError) as error:
        raise SystemExit(f"error: {error}")
