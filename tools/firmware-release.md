# Firmware Release Workflow

`firmware_release.py` builds, signs, publishes, and verifies browser-mediated
firmware updates. Keep the private P-256 key outside this repository and back it
up offline. The script verifies that it matches the public key compiled into the
firmware before preparing a release.

## Verify the Public Repository

```sh
python3 tools/firmware_release.py verify
```

This verifies the browser catalog, every signed release descriptor, CORS
headers, artifact sizes, and SHA-256 digests at
`https://firmware.opensprinkler.com`.

## Prepare a Release

```sh
python3 tools/firmware_release.py release \
  --private-key ~/secure/opensprinkler-firmware-p256.pem \
  --output-dir ~/opensprinkler-firmware/v1
```

The command imports the current catalog and verifies its signed releases,
builds ESP8266 and ESP32-C6, reads the version and build from `defines.h`, and
prepares architecture-specific immutable artifacts. Each release directory gets
a compact signed `release.json` descriptor. The unrestricted top-level
`manifest.json` is only a browser index. Nothing is published unless
`--destination` is supplied.

Use a distinct `--release-id` for test releases. Never replace an artifact under
an existing release ID; the tool rejects this automatically.

Publishing also refuses to remove an existing release from the browser index.
Use `--allow-prune` only when intentionally retiring test or release-candidate
entries; immutable files remain on the server unless removed separately.

## Publish

First inspect the remote operations:

```sh
python3 tools/firmware_release.py publish \
  --output-dir ~/opensprinkler-firmware/v1 \
  --destination USER@HOST:/ABSOLUTE/PATH/v1 \
  --dry-run
```

Remove `--dry-run` to publish. Release binaries, descriptors, and signatures are
synchronized first. The browser catalog is uploaded to a temporary name and
activated last, after which the tool downloads and verifies the public result.
The signing key is never transmitted.

For a single build-and-publish command, add the same `--destination` to
`release`. Use `--dry-run` for its first execution against a new server path.
