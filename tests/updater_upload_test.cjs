// Browser-level tests; every controller/catalog request is intercepted.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const zlib = require('node:zlib');
const { chromium } = require('playwright');
const root = path.resolve(__dirname, '..');
let html = fs.readFileSync(path.join(root, 'html/update.html'), 'utf8');
if (process.env.UPDATER_GENERATED === '1') {
    const header = fs.readFileSync(path.join(root, 'html/htmls.h'), 'utf8');
    const array = header.match(/const uint8_t update_html_gz\[\] PROGMEM = \{([\s\S]*?)\};/)[1];
    html = zlib.gunzipSync(Buffer.from(array.match(/0x[0-9a-f]{2}/g).map(n => parseInt(n, 16)))).toString();
}

(async () => {
    const browser = await chromium.launch({ headless: true,
        ...(process.env.BROWSER_PATH ? { executablePath: process.env.BROWSER_PATH } : {}) });
    try {
        const page = await browser.newPage();
        const errors = [];
        page.on('pageerror', e => errors.push(e.message));
        let uploads = 0, reply = {}, online = false, prepareReply = { result: 1, upload_token: 'token' }, checkStatus = 200;
        let releaseUpload;
        page.on('dialog', dialog => dialog.accept());
        await page.route('**/*', async route => {
            const url = new URL(route.request().url());
            if (url.pathname === '/update.html')
                return route.fulfill({ contentType: 'text/html', body: html });
            if (url.pathname === '/update/info')
                return route.fulfill({ json: { extension: '.bin', ap_mode: !online, configured: true,
                    target: 'esp8266', base_url: 'https://catalog.test', current_version: 221, current_build: 7 } });
            if (url.pathname === '/update/check') {
                assert.equal(route.request().method(), 'GET');
                assert(url.searchParams.has('pw') || url.searchParams.has('token'));
                return route.fulfill({ status: checkStatus, json: { result: checkStatus === 200 ? 1 : 2 } });
            }
            if (url.pathname === '/v1/manifest.json')
                return route.fulfill({ json: { schema: 2, releases: [{ id: 'test', version: 222, build: 0,
                    targets: ['esp8266'], descriptor: '/v1/releases/test/release.json', signature: '/v1/releases/test/release.sig' }] } });
            if (url.pathname === '/v1/releases/test/release.json')
                return route.fulfill({ json: { schema: 1, id: 'test', version: 222, build: 0,
                    targets: { esp8266: { file: '/v1/releases/test/firmware.bin', size: 2048, sha256: '0'.repeat(64) } } } });
            if (url.pathname === '/v1/releases/test/release.sig') return route.fulfill({ body: 'test-signature' });
            if (url.pathname === '/v1/releases/test/firmware.bin') return route.fulfill({ body: Buffer.alloc(2048) });
            if (url.pathname === '/update/prepare') {
                assert(url.searchParams.get('pw'));
                assert.equal(route.request().headers()['content-type'], 'application/octet-stream');
                assert.equal(JSON.parse(route.request().postData()).id, 'test');
                return route.fulfill({ status: prepareReply.result === 2 ? 401 : 200, json: prepareReply });
            }
            if (url.pathname === '/update' || url.pathname === '/update/upload') {
                ++uploads;
                assert.equal(route.request().method(), 'POST');
                if (url.pathname === '/update/upload') {
                    assert.equal(url.searchParams.get('token'), 'token');
                    assert.equal(url.searchParams.has('pw'), false);
                } else {
                    assert(url.searchParams.get('pw'));
                    assert.equal(url.searchParams.get('size'), '2048');
                    assert.equal(url.searchParams.get('sha256').length, 64);
                }
                const current = reply;
                if (current.hold) await new Promise(resolve => { releaseUpload = resolve; });
                if (current.abort) return route.abort();
                return route.fulfill({ status: current.status || 200, contentType: 'application/json',
                    body: current.raw ?? JSON.stringify(current.body || { result: 1 }) });
            }
            throw new Error('Unexpected network request: ' + url.origin + url.pathname);
        });
        const reset = async () => {
            await page.goto('http://updater.test/update.html');
            await page.waitForFunction(() => !document.getElementById('btn_submit').disabled);
            await page.locator('#pw').fill('password');
            await page.locator('#file').setInputFiles({ name: 'test.bin', mimeType: 'application/octet-stream', buffer: Buffer.alloc(2048) });
        };
        const message = async text => page.waitForFunction(text => document.getElementById('msg').textContent.includes(text), text);
        // Bad credentials/busy state are detected before sending an image body.
        for (const [status, text] of [[401, 'Authentication failed'], [409, 'busy']]) {
            await reset(); checkStatus = status; const count = uploads;
            await page.locator('#btn_submit').click(); await message(text);
            assert.equal(uploads, count);
            assert.equal(await page.locator('#btn_submit').isDisabled(), false);
        }
        checkStatus = 200;
        // Explicit refusals allow correction on the same page, without retrying themselves.
        for (const response of [
            { status: 401, text: 'Authentication failed' },
            { status: 409, text: 'busy' },
            { body: { result: 2 }, text: 'Authentication failed' },
            { body: { result: 64, item: 'Checksum rejected' }, text: 'Checksum rejected' }
        ]) {
            await reset(); reply = response; const before = uploads;
            await page.locator('#btn_submit').click(); await message(response.text);
            assert.equal(await page.locator('#btn_submit').isDisabled(), false);
            assert.equal(await page.locator('#pw').isDisabled(), false);
            assert.equal(uploads, before + 1);
        }
        // Successful installation remains locked even if a stale loader tries to unlock it.
        await reset(); reply = { hold: true }; const before = uploads;
        await page.locator('#btn_submit').click();
        await page.waitForFunction(() => uploadPending);
        await page.evaluate(async () => {
            await upload_firmware(new Blob(['x']), 'test.bin', 'password', false, '0'.repeat(64)).catch(() => {});
        });
        assert.equal(uploads, before + 1);
        releaseUpload(); await message('Update successful');
        await page.evaluate(() => set_busy(false));
        assert(await page.locator('#btn_submit').isDisabled());
        assert(await page.locator('#btn_install').isDisabled());

        // Ambiguous results never unlock or silently resend the image.
        for (const response of [{ abort: true }, { raw: 'bad JSON' }, { body: {} }, { status: 500 }]) {
            await reset(); reply = response; const count = uploads;
            await page.locator('#btn_submit').click(); await message('outcome is uncertain');
            await page.evaluate(() => set_busy(false));
            assert(await page.locator('#btn_submit').isDisabled());
            assert.equal(uploads, count + 1);
        }
        // Exercise the timeout event without waiting five minutes.
        await reset();
        await page.evaluate(() => {
            window.XMLHttpRequest = class {
                constructor() { this.upload = {}; }
                open() {}
                send() { queueMicrotask(() => this.ontimeout()); }
            };
        });
        await page.locator('#btn_submit').click(); await message('outcome is uncertain');
        assert(await page.locator('#btn_submit').isDisabled());
        // Signed descriptor admission is separate from upload. A rejected prepare is safe to correct.
        online = true; prepareReply = { result: 2 };
        await reset(); const initial = uploads;
        await page.locator('#btn_install').click(); await message('Authentication failed');
        assert.equal(uploads, initial);
        assert.equal(await page.locator('#btn_install').isDisabled(), false);
        prepareReply = { result: 1, upload_token: 'token' }; reply = {};
        await page.locator('#btn_install').click(); await message('Update successful');
        assert.equal(uploads, initial + 1);
        assert(await page.locator('#btn_install').isDisabled());
        assert.deepEqual(errors, []);
        console.log('PASS updater page: manual/signed upload, prepare rejection, explicit refusal, success, duplicate prevention, malformed/lost response and timeout');
    } finally { await browser.close(); }
})().catch(error => { console.error(error); process.exitCode = 1; });
