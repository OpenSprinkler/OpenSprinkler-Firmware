// Compare actual production-scheduler traces to the App's preview engine.
const fs = require('fs');
const vm = require('vm');
const assert = require('assert');
const [app] = process.argv.slice(2);
const context = { OSApp: {} };
vm.createContext(context);
vm.runInContext(fs.readFileSync(`${app}/www/js/modules/cycle-soak.js`, 'utf8'), context);
const fixtures = JSON.parse(fs.readFileSync(0, 'utf8'));
for (const fixture of fixtures) {
 const result = [];
 const engine = context.OSApp.CycleSoak.createPreview({
  capacity:255, delay:fixture.delay, remote:fixture.remote,
  policy:sid=>fixture.policies[sid], masters:()=>fixture.masters, minimum:()=>0
 }, segment=>result.push({...segment}));
 assert(engine.admit(fixture.requests, 1000));
 engine.advance(100000);
 const order = (a,b)=>a.st-b.st || a.sid-b.sid || a.pid-b.pid;
 assert.deepStrictEqual(result.sort(order), fixture.segments.sort(order), `scenario ${fixture.scenario}`);
}
console.log(`PASS ${fixtures.length} native scheduler / App preview shared traces`);
