# Firmware Update Server

OpenSprinkler's standalone update page retrieves release files through the user's browser. The
browser downloads the signed catalog and selected firmware from
`https://firmware.opensprinkler.com`, then uploads the unchanged bytes to the controller. The
controller verifies the catalog signature, hardware target, image header, file size, and SHA-256
digest before finalizing the update. The controller never makes an Internet connection, so this
works within the ESP8266 RAM limit without weakening update authentication.

## Initial Setup

1. Point the `firmware.opensprinkler.com` CNAME at the existing download host. Apache can map the
   hostname to a separate document root without changing the existing raysfiles downloads.
2. Configure an HTTPS virtual host with document root
   `/var/firmware.opensprinkler.com`. Keep Cloudflare proxying and HTTPS redirects enabled.
3. Add CORS response headers so a page served by the controller can read the catalog and binary.
4. Generate the release key once. Store at least two offline backups of the private key and commit
   only the generated public header.

```bash
python3 tools/firmware_release.py keygen \
  --private-key ~/secure/opensprinkler-firmware-p256.pem
```

Example Apache configuration:

```apache
<VirtualHost *:443>
    ServerName firmware.opensprinkler.com
    DocumentRoot /var/firmware.opensprinkler.com

    Include /etc/apache2/sites-common/ssl.conf
    SSLCertificateFile /etc/apache2/ssl/opensprinkler-origin-2026.crt
    SSLCertificateKeyFile /etc/apache2/ssl/opensprinkler-origin-2026.key
    AddType application/octet-stream .bin .bin32
    SetEnvIfNoCase Request_URI "\.(bin|bin32)$" no-gzip dont-vary

    <Directory /var/firmware.opensprinkler.com>
        Require all granted
        Options -Indexes
        Header always set Access-Control-Allow-Origin "*"
        Header always set Access-Control-Allow-Methods "GET, HEAD, OPTIONS"
    </Directory>

    <FilesMatch "^manifest\.(json|sig)$">
        Header always set Cache-Control "no-store, no-cache, must-revalidate"
    </FilesMatch>

    <Location /v1/releases/>
        Header always set Cache-Control "public, max-age=31536000, immutable"
    </Location>
</VirtualHost>
```

Put this in a dedicated site such as `/etc/apache2/sites-available/zz-firmware.conf`, enable it,
then validate and reload Apache:

```bash
sudo a2ensite zz-firmware.conf
sudo apache2ctl configtest
sudo systemctl reload apache2
```

This configuration requires `mod_headers`. The existing wildcard OpenSprinkler origin certificate
may be reused if it covers `*.opensprinkler.com`. Binary files must be served unchanged with a
valid `Content-Length`; disable content rewriting and compression for them. Cloudflare security
features can remain enabled because the client is a normal browser. If a challenge is ever applied
to these static files, create the narrowest possible exception for `/v1/*` on this hostname.

## Publish a Release

Build both targets, then prepare a signed publication tree:

```bash
python3 tools/firmware_release.py prepare \
  --private-key ~/secure/opensprinkler-firmware-p256.pem \
  --output-dir ~/firmware-publish/v1 \
  --esp8266 .pio/build/os3x_esp8266/firmware.bin \
  --esp32c6 .pio/build/os4_esp32c6/firmware.bin \
  --version 221 --build 6 --sequence 1
```

The tool gives ESP8266 files a `.bin` extension and ESP32-C6 files a `.bin32` extension, calculates SHA-256, keeps at most three catalog releases, and signs the exact catalog bytes. The catalog contains only the fields required to identify, validate, and install each release; the update page derives its display label from the numeric version and build. Increase `--sequence` for every publication; controllers persist the highest verified sequence and reject replayed older catalogs. Publish immutable artifacts first and the catalog last:

The generated catalog is limited to 1900 bytes, matching the verifier limit on ESP8266 and
ESP32-C6. Do not raise the release-tool limit without changing and testing the firmware limit.

```bash
rsync -av --ignore-existing ~/firmware-publish/v1/releases/ \
  server:/var/firmware.opensprinkler.com/v1/releases/
rsync -av ~/firmware-publish/v1/manifest.json ~/firmware-publish/v1/manifest.sig \
  server:/var/firmware.opensprinkler.com/v1/
```

Verify the public result before announcing a release:

```bash
curl -f https://firmware.opensprinkler.com/v1/manifest.json
curl -f https://firmware.opensprinkler.com/v1/manifest.sig
curl -I https://firmware.opensprinkler.com/v1/releases/2.2.1-6/opensprinkler-2.2.1-6-esp8266.bin
```

Never upload the private signing key to the web server. If it is lost, existing firmware cannot trust a replacement key without a manually installed transition release. If it is exposed, rotate it immediately through a firmware release signed by the old key.
