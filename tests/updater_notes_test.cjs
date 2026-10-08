// Run with Playwright on NODE_PATH; see tests/manual/updater-release-notes.md.
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
const fixtures = require('./fixtures/updater-release-notes.json').releases;
const screenshots = process.env.UPDATER_SCREENSHOTS;

(async () => {
    const browser = await chromium.launch({
        headless: true,
        ...(process.env.BROWSER_PATH ? { executablePath: process.env.BROWSER_PATH } : {})
    });
    try {
        const page = await browser.newPage();
        const unexpected = [], errors = [];
        let reply = { status: 200, data: fixtures[0] }, requests = 0;
        let releaseSlow;
        page.on('pageerror', error => errors.push(error.message));
        await page.route('**/*', async route => {
            const url = new URL(route.request().url());
            const json = data => route.fulfill({ contentType: 'application/json', body: JSON.stringify(data) });
            if (url.href === 'http://updater.test/update.html')
                return route.fulfill({ contentType: 'text/html', body: html });
            if (url.href === 'http://updater.test:8080/update/info')
                return json({ extension: '.bin', configured: true, base_url: 'https://catalog.test',
                    target: 'esp8266', current_version: 221, current_build: 6 });
            if (url.href === 'https://catalog.test/v1/manifest.json')
                return json({ schema: 2, releases: [{ id: '2.2.1-6', version: 221, build: 6,
                    targets: ['esp8266'], descriptor: '/v1/releases/test/release.json', signature: '/v1/releases/test/release.sig' }] });
            if (url.href === 'https://catalog.test/v1/releases/test/release.json')
                return json({ schema: 1, id: '2.2.1-6', version: 221, build: 6,
                    targets: { esp8266: { file: '/v1/releases/test/firmware.bin', size: 42, sha256: '0'.repeat(64) } } });
            if (url.href === 'https://catalog.test/v1/releases/test/release.sig')
                return route.fulfill({ body: 'mock-descriptor-signature' });
            if (url.hostname === 'api.github.com') {
                requests++;
                assert.equal(route.request().headers().accept, 'application/vnd.github.full+json');
                const current = reply;
                if (current.slow) await new Promise(resolve => { releaseSlow = resolve; });
                if (current.abort) return route.abort();
                return route.fulfill({ status: current.status, contentType: 'application/json',
                    body: current.raw ?? JSON.stringify(current.data) });
            }
            unexpected.push(url.href);
            return route.abort();
        });
        await page.goto('http://updater.test/update.html');
        await page.waitForFunction(() => !document.getElementById('btn_notes').disabled);
        const notes = page.locator('#notes_body');
        const open = () => page.locator('#btn_notes').click();
        const close = () => page.keyboard.press('Escape');
        const clearCache = () => page.evaluate(() => notes_cache.clear());
        const waitText = value => page.waitForFunction(text => document.getElementById('notes_body').textContent.includes(text), value);
        const render = data => page.evaluate(data => render_release_notes(document.getElementById('notes_body'), data), data);

        // Exercise fetch, descriptor loading, real rendered notes and cache reuse.
        for (const fixture of fixtures) {
            reply = { status: 200, data: fixture };
            await clearCache();
            const before = requests;
            await open();
            await waitText(fixture.body_text.trim().slice(0, 40));
            assert.equal(await notes.evaluate(el => el.classList.contains('plain')), false);
            const actual = await notes.textContent();
            for (const line of fixture.body_text.split('\n').filter(line => line.trim()))
                assert(actual.replace(/\s+/g, ' ').includes(line.replace(/\s+/g, ' ').trim()), fixture.tag_name + ': ' + line);
            await close();
            assert.equal(await page.evaluate(() => document.activeElement.id), 'btn_notes');
            await open();
            await waitText(fixture.body_text.trim().slice(0, 40));
            assert.equal(requests, before + 1, 'Reopening notes should reuse the cache');
            await close();
        }
        console.log('PASS four real GitHub releases and session cache');

        const hostile = '<p id="pw" class="button-link" style="position:fixed" onclick="window.pwned=1">Safe text</p>' +
            '<script>window.pwned=1</script><style>body{display:none}</style>' +
            '<iframe src="https://evil.test/frame" srcdoc="<script>parent.pwned=1</script>"></iframe>' +
            '<object data="https://evil.test/object"></object><embed src="https://evil.test/embed">' +
            '<svg onload="window.pwned=1"><a href="https://evil.test/svg">SVG</a></svg>' +
            '<form action="https://evil.test/form"><input name="pw"><button>Submit</button></form>' +
            '<base href="https://evil.test/"><meta http-equiv="refresh" content="0;url=https://evil.test/refresh">' +
            '<link rel="stylesheet" href="https://evil.test/style"><template><img src="https://evil.test/template"></template>' +
            '<img src="https://evil.test/image" srcset="https://evil.test/srcset 2x" onerror="window.pwned=1" alt="Image description">' +
            '<div><g-emoji alias="seedling">Plant</g-emoji></div><a href="#heading"><svg></svg></a>' +
            '<input type="checkbox" checked><input type="checkbox">' +
            '</template><img src="https://evil.test/template-breakout" onerror="window.pwned=1">' +
            '<math><mtext><table><mglyph><style><!--</style><img title="--><img src=https://evil.test/mxss onerror=window.pwned=1>">';
        await render({ body_html: hostile, body: 'fallback' });
        assert((await notes.textContent()).includes('Safe text'));
        assert((await notes.textContent()).includes('Image description'));
        assert((await notes.textContent()).includes('Plant'));
        assert((await notes.textContent()).includes('[x] [ ]'));
        assert.equal(await notes.locator('script,style,iframe,object,embed,svg,math,form,input,template,img,base,meta,link').count(), 0);
        assert.equal(await notes.locator('[id],[style],[onclick],[class]').count(), 0);
        assert.equal(await page.evaluate(() => window.pwned), undefined);

        const badLinks = ['javascript:alert(1)', 'java&#x09;script:alert(1)', '&#106;avascript:alert(1)',
            'java\nscript:alert(1)', '&#1;javascript:alert(1)', 'data:text/html,hello', 'vbscript:msgbox(1)',
            'http://example.com', '//example.com', '/relative', '#heading', ' https://example.com',
            'https:\\example.com', 'https://user:password@example.com'];
        await render({ body_html: badLinks.map(href => `<a href="${href}" onmouseover="window.pwned=1">bad</a>`).join('') +
            '<a href="HTTPS://example.com/path?q=1&amp;x=2" data-info="no" download="bad">Good</a>' });
        assert.equal(await notes.locator('a').count(), 1);
        assert.deepEqual(await notes.locator('a').evaluate(el => Object.fromEntries([...el.attributes].map(a => [a.name, a.value]))),
            { href: 'https://example.com/path?q=1&x=2', target: '_blank', rel: 'noopener noreferrer' });
        assert.equal(await notes.textContent(), 'bad'.repeat(badLinks.length) + 'Good');
        console.log('PASS hostile HTML, inert resources, strict attributes and HTTPS links');

        for (const data of [
            { body: '<img onerror=alert(1)> Plain Markdown **bold**' },
            { body_text: 'Plain text', body: '**Markdown**' },
            { body_html: 'x'.repeat(65537), body: 'Too long' },
            { body_html: '<div>'.repeat(40) + 'Nested' + '</div>'.repeat(40), body: 'Too deep' },
            { body_html: '<br>'.repeat(4100), body: 'Too many nodes' },
            { body_html: '<script>bad()</script>', body: 'No safe HTML' },
            { body_html: null, body: null }
        ]) {
            await render(data);
            assert.equal(await notes.evaluate(el => el.classList.contains('plain')), true);
            assert.equal(await notes.textContent(), data.body_text || data.body || 'No release notes were provided for this version.');
            assert.equal(await notes.locator('*').count(), 0);
        }
        await render({ body: 'x'.repeat(70000) });
        assert((await notes.textContent()).includes('Notes truncated.'));
        assert((await notes.textContent()).length < 65700);

        for (const failure of [
            { status: 404, data: {}, text: 'have not been published' },
            { status: 403, data: {}, text: 'GitHub returned HTTP 403' },
            { status: 500, data: {}, text: 'GitHub returned HTTP 500' },
            { status: 200, raw: 'invalid JSON', text: 'Unable to load release notes' },
            { status: 200, data: null, text: 'Invalid release notes response' },
            { abort: true, text: 'Unable to load release notes' }
        ]) {
            reply = failure;
            await clearCache();
            await open();
            await waitText(failure.text);
            await close();
        }
        console.log('PASS plain fallback, limits, unpublished notes and network failures');

        // A previous dialog's slow response must not overwrite the newly selected notes.
        await clearCache();
        reply = { status: 200, data: { body: 'OLD RESPONSE' }, slow: true };
        await open();
        while (!releaseSlow) await page.waitForTimeout(10);
        await close();
        reply = { status: 200, data: fixtures[0] };
        await open();
        await waitText('Main changes');
        releaseSlow();
        await page.waitForTimeout(100);
        assert(!(await notes.textContent()).includes('OLD RESPONSE'));
        await close();

        await clearCache();
        const beforeEviction = requests;
        await page.evaluate(async () => {
            for (let i = 0; i < 9; i++) await fetch_release_notes('cache-' + i);
        });
        assert.equal(await page.evaluate(() => notes_cache.size), 8);
        await page.evaluate(() => fetch_release_notes('cache-8'));
        assert.equal(requests, beforeEviction + 9);
        await page.evaluate(() => fetch_release_notes('cache-0'));
        assert.equal(requests, beforeEviction + 10);

        // Build predictable synthetic layout coverage in addition to actual release screenshots.
        const formatted = '<h1>Release notes</h1><p><b>Important:</b> Back up configuration.</p>' +
            '<h3>Changes</h3><ol start="3"><li>First</li><li><i>Second</i><ul><li>Nested</li></ul></li></ol>' +
            '<blockquote>Compatibility note</blockquote><hr><p><del>Old</del> <tt>/jc</tt> ' +
            '<a href="https://example.com/' + 'x'.repeat(300) + '">https://example.com/' + 'x'.repeat(300) + '</a></p>' +
            '<pre><code>' + '0123456789 '.repeat(60) + '</code></pre><table><thead><tr>' +
            '<th>Heading</th>'.repeat(12) + '</tr></thead><tbody><tr>' + '<td>Details</td>'.repeat(12) + '</tr></tbody></table>';
        if (screenshots) fs.mkdirSync(screenshots, { recursive: true });
        for (const width of [390, 1280]) {
            await page.setViewportSize({ width, height: width === 390 ? 844 : 900 });
            for (const scheme of ['light', 'dark']) {
                await page.emulateMedia({ colorScheme: scheme });
                await clearCache();
                reply = { status: 200, data: fixtures[0] };
                await open();
                await waitText('Main changes');
                if (screenshots) await page.screenshot({ path: path.join(screenshots, `notes-${width}-${scheme}.png`) });
                await render({ body_html: formatted });
                assert.equal(await notes.locator('ol').getAttribute('start'), '3');
                assert.equal(await notes.locator('table').count(), 1);
                assert.equal(await page.evaluate(() => document.documentElement.scrollWidth <= window.innerWidth), true);
                assert.equal(await notes.evaluate(el => el.scrollWidth <= el.clientWidth), true, 'Wide content escapes notes');
                assert.equal(await notes.locator('pre').evaluate(el => el.scrollWidth > el.clientWidth), true);
                assert.equal(await notes.locator('.notes-table').evaluate(el => el.scrollWidth > el.clientWidth), true);
                const panel = await page.locator('.modal-panel').boundingBox();
                assert(panel.x >= 0 && panel.x + panel.width <= width && panel.y >= 0);
                await page.locator('#btn_notes_close').focus();
                await page.keyboard.press('Shift+Tab');
                assert.equal(await page.evaluate(() => document.activeElement.id), 'notes_github');
                await page.keyboard.press('Tab');
                assert.equal(await page.evaluate(() => document.activeElement.id), 'btn_notes_close');
                assert.equal(await page.evaluate(() => document.querySelector('main').inert), true);
                await notes.evaluate(el => { el.scrollTop = el.scrollHeight; });
                if (screenshots) await page.screenshot({ path: path.join(screenshots, `notes-wide-${width}-${scheme}.png`) });
                await close();
                assert.equal(await page.evaluate(() => document.querySelector('main').inert), false);
            }
        }
        await open();
        await waitText('Main changes');
        await page.locator('#btn_notes_close').click();
        assert.equal(await page.locator('#notes_modal').isHidden(), true);
        await open();
        await waitText('Main changes');
        await page.locator('#notes_modal').click({ position: { x: 2, y: 2 } });
        assert.equal(await page.locator('#notes_modal').isHidden(), true);
        assert.deepEqual(unexpected, [], 'No firmware downloads, controller writes, or embedded-resource requests expected');
        assert.deepEqual(errors, []);
        console.log('PASS desktop/mobile light/dark layout, tables/code, focus and stale requests');
    } finally {
        await browser.close();
    }
})().catch(error => { console.error(error); process.exitCode = 1; });
