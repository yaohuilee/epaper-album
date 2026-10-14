const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

// Execute the production page script, including request admission and completion polling.
async function scenario(result, expected, warning, engine) {
  const nodes = new Map(), messages = [], requests = [];
  function node(id) {
    if (!nodes.has(id)) nodes.set(id, {
      children: [], value: '', checked: false, dataset: {},
      querySelectorAll: () => [], querySelector: () => node(id + '-child'),
      addEventListener() {}, replaceChildren(...items) { this.children = items; },
      append() {}, close() {}, showModal() {}
    });
    return nodes.get(id);
  }
  const status = {
    control: {token: 'test-token', busy: false, job_id: 0, state: 'idle'},
    announcement: {
      ready: true, enabled: true, hourly: true, half_hourly: true,
      volume_percent: 40, pause_periods: [], phase: 'idle', busy: false,
      voice_pack: 'test-pack', test_available: true, completed_test_id: 0,
      tts_available: true, tts_test_available: true
    }
  };
  const context = {
    document: {getElementById: node, body: {append() {}}}, window: {},
    latest: status, available: true, TextEncoder, AbortSignal,
    beijing: String, refresh: async () => {},
    saveMessage: (...args) => messages.push(args),
    fetch: async (url, options) => {
      requests.push({url, body: JSON.parse(options.body)});
      return {ok: true, json: async () => ({job_id: 1})};
    }
  };
  vm.runInNewContext(fs.readFileSync(path.join(__dirname, '../components/services/announcements.js'), 'utf8'), context);
  node('sound-volume').value = '55';
  await node('test-hour').onclick();
  const body = {kind: 'hour', volume_percent: 55};
  assert.deepEqual(requests, [{url: '/api/announcement-test', body}]);
  status.control = {...status.control, job_id: 1, state: 'succeeded'};
  status.announcement.test_request_id = 1;
  context.window.updateAnnouncement(status);
  assert.equal(messages.at(-1)[1], '正在试听…'); // Queue success is not playback success.
  status.announcement.completed_test_id = 1;
  status.announcement.last_test_result = result;
  status.announcement.last_test_error = result === 'error' ? 'Injected missing clip' : '';
  context.window.updateAnnouncement(status);
  assert.equal(messages.at(-1)[1], expected);
  assert.equal(messages.at(-1)[2], warning);
  const count = messages.length;
  context.window.updateAnnouncement(status);
  assert.equal(messages.length, count); // Completion toast is emitted once.
}
(async () => {
  await scenario('succeeded', '试听已完成。', false);
  await scenario('partial', '时间播报完成，温湿度播报未完成。', true);
  await scenario('error', '试听未完成：Injected missing clip', true);
  await scenario('skipped', '试听未完成：skipped', true);
  console.log('announcement UI tests: 4 completion scenarios passed');
})().catch(error => { console.error(error); process.exitCode = 1; });
