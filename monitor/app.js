// Bedside monitor for the ESP32-S3 classifier.
// The GATT contract below must match the firmware; see docs/BLE_PROTOCOL.md.

const SERVICE_UUID = '8e7a1f20-1d0b-4f6a-9c3e-2b5c7d9e1001';
const STATE_UUID = '8e7a1f21-1d0b-4f6a-9c3e-2b5c7d9e1001';

const STATES = {
  calm: { label: 'calm breathing', colour: '#1faa4b' },
  fast: { label: 'fast breathing', colour: '#e03131' },
  not_attached: { label: 'not attached', colour: '#2f6fd6' },
  shaking: { label: 'shaking', colour: '#b833c9' },
  unknown: { label: 'no data', colour: '#555a63' },
};

const TIMELINE_WINDOW_MS = 5 * 60 * 1000;
const TICK_MS = 500;
const RECONNECT_DELAYS_MS = [2000, 4000, 8000];

const elements = {
  unsupported: document.getElementById('unsupported'),
  unsupportedReason: document.getElementById('unsupported-reason'),
  connectButton: document.getElementById('connect-button'),
  simulateButton: document.getElementById('simulate-button'),
  dropButton: document.getElementById('drop-button'),
  silenceButton: document.getElementById('silence-button'),
  alarmSilence: document.getElementById('alarm-silence'),
  connectionStatus: document.getElementById('connection-status'),
  stateName: document.getElementById('state-name'),
  stateSince: document.getElementById('state-since'),
  probabilities: document.getElementById('probabilities'),
  fastLimit: document.getElementById('fast-limit'),
  notAttachedLimit: document.getElementById('not-attached-limit'),
  staleLimit: document.getElementById('stale-limit'),
  checkFast: document.getElementById('check-fast'),
  checkNotAttached: document.getElementById('check-not-attached'),
  checkStale: document.getElementById('check-stale'),
  checkConnection: document.getElementById('check-connection'),
  checkSound: document.getElementById('check-sound'),
  timeline: document.getElementById('timeline'),
  totalsBody: document.querySelector('#totals-table tbody'),
  eventLog: document.getElementById('event-log'),
  alarmOverlay: document.getElementById('alarm-overlay'),
  alarmTitle: document.getElementById('alarm-title'),
  alarmDetail: document.getElementById('alarm-detail'),
};

// Runtime state.
let mode = 'idle'; // 'idle' | 'ble' | 'sim'
let linkUp = false;
let device = null;
let reconnectAttempt = 0;
let reconnectTimer = null;
let currentState = 'unknown';
let lastProbabilities = null;
let lastUpdateMs = 0;
let segments = []; // { state, startMs, endMs|null }
let activeAlarms = new Map(); // alarm name -> reason shown in the overlay
let silenced = false;
let audioContext = null;
let beepTimer = null;
let simulateTimer = null;

function log(message, warn) {
  const item = document.createElement('li');
  const time = new Date().toLocaleTimeString();
  item.textContent = time + '  ' + message;
  if (warn) item.className = 'warn';
  elements.eventLog.prepend(item);
  while (elements.eventLog.children.length > 200) elements.eventLog.lastChild.remove();
}

function checkWebBluetooth() {
  if (!window.isSecureContext) {
    // Web Bluetooth only exists in a secure context: https:// or http://localhost.
    return 'This page was opened over plain HTTP, so the browser hides the Bluetooth API. ' +
      'Open the GitHub Pages address (https) or run the page on http://localhost.';
  }
  if (!navigator.bluetooth) {
    return 'navigator.bluetooth is missing. Use Chrome or Edge on Windows, macOS or Android; ' +
      'Firefox and Safari do not support Web Bluetooth.';
  }
  return null;
}

function numberInput(input, fallback) {
  const value = parseFloat(input.value);
  return isFinite(value) && value > 0 ? value : fallback;
}

function setConnectionStatus(text, cssClass) {
  elements.connectionStatus.textContent = text;
  elements.connectionStatus.className = 'status' + (cssClass ? ' ' + cssClass : '');
}

function parsePayload(text) {
  const trimmed = String(text).trim();
  if (trimmed.startsWith('{')) {
    try {
      const payload = JSON.parse(trimmed);
      const probabilities = payload.probs || payload.p || null;
      return { state: normalizeState(payload.state || payload.s), probabilities: probabilities };
    } catch (error) {
      // Fall through: treat the text as a bare label.
    }
  }
  const fields = trimmed.split(/[,\s]+/);
  const confidence = fields.length > 1 ? parseFloat(fields[1]) : NaN;
  const probabilities = isFinite(confidence) ? { confidence: confidence } : null;
  return { state: normalizeState(fields[0]), probabilities: probabilities };
}

function normalizeState(raw) {
  const key = String(raw || '').toLowerCase().trim();
  if (STATES[key]) return key;
  const aliases = { calm_breathing: 'calm', breathing_calm: 'calm', breathing: 'calm',
                    fast_breathing: 'fast', breathing_fast: 'fast',
                    hand: 'not_attached', off: 'not_attached', idle: 'not_attached',
                    shake: 'shaking' };
  return aliases[key] || 'unknown';
}

function recordState(state, probabilities) {
  const now = Date.now();
  lastUpdateMs = now;

  if (state !== currentState) {
    const openSegment = segments[segments.length - 1];
    if (openSegment && openSegment.endMs === null) openSegment.endMs = now;
    segments.push({ state: state, startMs: now, endMs: null });
    log('state -> ' + STATES[state].label);
    currentState = state;
  }
  if (probabilities) lastProbabilities = probabilities;
  renderStateCard();
}

// Also redraws the duration counter on every tick, which is why it takes no argument:
// the elapsed time changes even when no new state arrives.
function renderStateCard() {
  const state = STATES[currentState];
  elements.stateName.textContent = state.label;
  elements.stateName.style.color = state.colour;
  const openSegment = segments[segments.length - 1];
  if (openSegment) {
    elements.stateSince.textContent = 'for ' + formatDuration(Date.now() - openSegment.startMs);
  }
  renderProbabilities(lastProbabilities);
}

function renderProbabilities(probabilities) {
  if (!probabilities) {
    elements.probabilities.innerHTML = '';
    return;
  }
  const entries = Object.entries(probabilities).slice(0, 6);
  elements.probabilities.innerHTML = entries.map(function (entry) {
    const name = entry[0];
    const value = Math.max(0, Math.min(1, Number(entry[1]) || 0));
    const colour = STATES[name] ? STATES[name].colour : '#7c8794';
    return '<div class="prob"><div>' + name + ' ' + Math.round(value * 100) + '%</div>' +
      '<div class="prob-bar"><div class="prob-fill" style="width:' + (value * 100) +
      '%;background:' + colour + '"></div></div></div>';
  }).join('');
}

function formatDuration(ms) {
  const totalSeconds = Math.floor(ms / 1000);
  const minutes = Math.floor(totalSeconds / 60);
  const seconds = totalSeconds % 60;
  return minutes > 0 ? minutes + ' min ' + seconds + ' s' : seconds + ' s';
}

function openSegment() {
  return segments[segments.length - 1] || null;
}

function evaluateAlarms() {
  const now = Date.now();
  const segment = openSegment();
  const heldMs = segment ? now - segment.startMs : 0;

  const fastLimitMs = numberInput(elements.fastLimit, 30) * 1000;
  check(elements.checkFast, currentState === 'fast' && heldMs >= fastLimitMs,
        'Fast breathing', 'held ' + formatDuration(heldMs) + ', limit ' + formatDuration(fastLimitMs));

  const notAttachedLimitMs = numberInput(elements.notAttachedLimit, 60) * 1000;
  check(elements.checkNotAttached, currentState === 'not_attached' && heldMs >= notAttachedLimitMs,
        'Not attached', 'held ' + formatDuration(heldMs) + ', limit ' + formatDuration(notAttachedLimitMs));

  const staleLimitMs = numberInput(elements.staleLimit, 5) * 1000;
  const staleMs = now - lastUpdateMs;
  check(elements.checkStale, linkUp && lastUpdateMs > 0 && staleMs >= staleLimitMs,
        'No data', 'no update for ' + formatDuration(staleMs) + ', limit ' + formatDuration(staleLimitMs));

  renderAlarmState();
}

// An alarm stays raised while its condition holds; silencing stops the sound but
// keeps the banner, so a cleared condition still has to be re-triggered to alarm again.
function check(checkbox, condition, title, detail) {
  if (!checkbox.checked) {
    activeAlarms.delete(title);
    return;
  }
  if (condition && !activeAlarms.has(title)) {
    activeAlarms.set(title, detail);
    silenced = false;
    log('ALARM: ' + title + ' - ' + detail, true);
  } else if (!condition) {
    activeAlarms.delete(title);
  }
}

function raiseConnectionAlarm(detail) {
  if (!elements.checkConnection.checked) return;
  activeAlarms.set('Connection lost', detail);
  silenced = false;
  log('ALARM: Connection lost - ' + detail, true);
  renderAlarmState();
}

function renderAlarmState() {
  // The connection alarm is only cleared by the Silence button, never by time,
  // because the board may be powered off and nobody should be left without sound silently.
  const visible = [...activeAlarms.entries()];
  if (visible.length === 0) {
    elements.alarmOverlay.classList.add('hidden');
    elements.silenceButton.classList.add('hidden');
    stopBeep();
    return;
  }
  elements.alarmTitle.textContent = visible.length === 1 ? visible[0][0].toUpperCase() : 'ALARMS (' + visible.length + ')';
  elements.alarmDetail.textContent = visible.map(function (alarm) {
    return visible.length === 1 ? alarm[1] : alarm[0] + ': ' + alarm[1];
  }).join('   ·   ');
  elements.alarmOverlay.classList.remove('hidden');
  elements.silenceButton.classList.remove('hidden');
  if (silenced) stopBeep();
  else startBeep();
}

function silenceAlarms() {
  silenced = true;
  stopBeep();
  elements.alarmOverlay.classList.add('hidden');
  elements.silenceButton.classList.add('hidden');
  log('alarm silenced');
}

function startBeep() {
  if (beepTimer || !elements.checkSound.checked) return;
  audioContext = audioContext || new (window.AudioContext || window.webkitAudioContext)();
  if (audioContext.state === 'suspended') audioContext.resume();
  beepTimer = setInterval(playBeep, 700);
  playBeep();
}

function playBeep() {
  const oscillator = audioContext.createOscillator();
  const gain = audioContext.createGain();
  oscillator.type = 'square';
  oscillator.frequency.value = 880;
  gain.gain.value = 0.12;
  oscillator.connect(gain).connect(audioContext.destination);
  oscillator.start();
  oscillator.stop(audioContext.currentTime + 0.18);
}

function stopBeep() {
  if (beepTimer) clearInterval(beepTimer);
  beepTimer = null;
}

function drawTimeline() {
  const canvas = elements.timeline;
  const context = canvas.getContext('2d');
  const dpr = window.devicePixelRatio || 1;
  const width = canvas.clientWidth || canvas.width;
  const height = 120;
  if (canvas.width !== Math.round(width * dpr)) {
    canvas.width = Math.round(width * dpr);
    canvas.height = Math.round(height * dpr);
  }
  context.setTransform(dpr, 0, 0, dpr, 0, 0);
  context.clearRect(0, 0, width, height);

  const now = Date.now();
  const startMs = now - TIMELINE_WINDOW_MS;
  const timeToX = function (ms) { return ((ms - startMs) / TIMELINE_WINDOW_MS) * width; };

  for (const segment of segments) {
    if ((segment.endMs || now) < startMs) continue;
    const x = Math.max(0, timeToX(segment.startMs));
    const end = Math.min(width, timeToX(segment.endMs || now));
    if (end <= x) continue;
    context.fillStyle = STATES[segment.state].colour;
    context.fillRect(x, 34, end - x - 1, 34);
  }

  context.fillStyle = '#97a1af';
  context.font = '11px system-ui, sans-serif';
  for (let minutesAgo = 5; minutesAgo >= 0; minutesAgo--) {
    const markMs = now - minutesAgo * 60000;
    const x = timeToX(markMs);
    context.fillRect(x, 30, 1, 42);
    const label = minutesAgo === 0 ? 'now' : '-' + minutesAgo + ' min';
    context.fillText(label, Math.min(width - 34, Math.max(0, x - 10)), 24);
  }

  if (segments.length === 0) {
    context.fillStyle = '#555a63';
    context.fillText('no states recorded yet', 8, 55);
  }
}

function renderTotals() {
  const now = Date.now();
  const totals = {};
  let totalMs = 0;
  for (const segment of segments) {
    const duration = (segment.endMs || now) - segment.startMs;
    totals[segment.state] = (totals[segment.state] || 0) + duration;
    totalMs += duration;
  }
  const rows = Object.keys(STATES).filter(function (key) { return totals[key]; }).map(function (key) {
    const duration = totals[key];
    const share = totalMs > 0 ? Math.round((duration / totalMs) * 100) : 0;
    return '<tr><td><span class="swatch" style="background:' + STATES[key].colour + '"></span>' +
      STATES[key].label + '</td><td>' + formatDuration(duration) + '</td><td>' + share + '%</td></tr>';
  });
  elements.totalsBody.innerHTML = rows.length ? rows.join('') : '<tr><td colspan="3">no data yet</td></tr>';
}

async function connectBoard() {
  const reason = checkWebBluetooth();
  if (reason) {
    elements.unsupportedReason.textContent = ' ' + reason;
    elements.unsupported.classList.remove('hidden');
    return;
  }
  try {
    setConnectionStatus('choosing device...', '');
    // requestDevice must be called straight from a user gesture, which is why it is
    // only ever reached from the Connect button click.
    device = await navigator.bluetooth.requestDevice({
      filters: [{ services: [SERVICE_UUID] }, { namePrefix: 'ESP32' }, { namePrefix: 'MPA-PRM' }],
      optionalServices: [SERVICE_UUID],
    });
    await subscribeToBoard();
  } catch (error) {
    setConnectionStatus('not connected', 'offline');
    log('connect failed: ' + error.message, true);
  }
}

async function subscribeToBoard() {
  device.addEventListener('gattserverdisconnected', onBoardDisconnected);
  setConnectionStatus('connecting...', '');
  const server = await device.gatt.connect();
  const service = await server.getPrimaryService(SERVICE_UUID);
  const characteristic = await service.getCharacteristic(STATE_UUID);
  await characteristic.startNotifications();
  characteristic.addEventListener('characteristicvaluechanged', onNotification);
  mode = 'ble';
  linkUp = true;
  reconnectAttempt = 0;
  lastUpdateMs = Date.now();
  setConnectionStatus('connected: ' + (device.name || 'board'), 'online');
  log('connected to ' + (device.name || 'board'));
  elements.dropButton.classList.remove('hidden');
  // Read once so the state is correct immediately instead of after the first change.
  try {
    const value = await characteristic.readValue();
    onNotification({ target: { value: value } });
  } catch (error) {
    // Not fatal: notifications will arrive anyway.
  }
}

function onNotification(event) {
  const text = new TextDecoder('utf-8').decode(event.target.value);
  const parsed = parsePayload(text);
  recordState(parsed.state, parsed.probabilities);
}

function onBoardDisconnected() {
  linkUp = false;
  mode = 'idle';
  setConnectionStatus('disconnected', 'offline');
  log('board disconnected', true);
  raiseConnectionAlarm('the board stopped advertising or lost power');
  scheduleReconnect();
}

function scheduleReconnect() {
  if (reconnectAttempt >= RECONNECT_DELAYS_MS.length || !device) return;
  const delay = RECONNECT_DELAYS_MS[reconnectAttempt];
  reconnectAttempt += 1;
  setConnectionStatus('reconnecting in ' + delay / 1000 + ' s (' + reconnectAttempt + '/' + RECONNECT_DELAYS_MS.length + ')', '');
  reconnectTimer = setTimeout(async function () {
    try {
      await subscribeToBoard();
    } catch (error) {
      scheduleReconnect();
    }
  }, delay);
}

const SIMULATE_SCENARIO = [
  ['calm', 6], ['calm', 6], ['calm', 6], ['calm', 6],
  ['fast', 5], ['fast', 5], ['fast', 5],
  ['shaking', 3],
  ['not_attached', 8], ['not_attached', 8], ['not_attached', 8], ['not_attached', 8], ['not_attached', 8], ['not_attached', 8],
];
let simulateIndex = 0;
let pinnedState = null; // set by ?simulate=fast to hold one state, for testing the alarms

// A plausible softmax-looking distribution so the probability bars can be shown
// and checked without a board. The real probabilities come from the board.
function simulatedProbabilities(winner) {
  const confidence = 0.72 + Math.random() * 0.26;
  const keys = Object.keys(STATES).filter(function (key) { return key !== 'unknown'; });
  const others = keys.filter(function (key) { return key !== winner; });
  const share = (1 - confidence) / others.length;
  const probabilities = {};
  for (const key of keys) probabilities[key] = key === winner ? confidence : share;
  return probabilities;
}

function startSimulation() {
  // Lets the timeline and both duration alarms be tested with no board attached,
  // which is how the page was validated before hardware was available.
  mode = 'sim';
  linkUp = true;
  lastUpdateMs = Date.now();
  elements.connectButton.disabled = true;
  elements.simulateButton.textContent = 'Stop simulation';
  elements.dropButton.classList.remove('hidden');
  setConnectionStatus('simulated board', 'online');
  log('simulation started (short alarm limits are easier to see: try 3 s)');
  simulateTimer = setInterval(function () {
    const state = pinnedState || SIMULATE_SCENARIO[simulateIndex % SIMULATE_SCENARIO.length][0];
    simulateIndex += 1;
    recordState(state, simulatedProbabilities(state));
  }, 1000);
}

function stopSimulation(raiseAlarm) {
  if (simulateTimer) clearInterval(simulateTimer);
  simulateTimer = null;
  mode = 'idle';
  linkUp = false;
  elements.connectButton.disabled = false;
  elements.simulateButton.textContent = 'Simulate';
  elements.dropButton.classList.add('hidden');
  setConnectionStatus('not connected', '');
  if (raiseAlarm) raiseConnectionAlarm('simulation: radio drop');
  else log('simulation stopped');
}

function tick() {
  renderStateCard();
  evaluateAlarms();
  drawTimeline();
  renderTotals();
}

// Query parameters exist to rehearse and test each alarm in seconds instead of
// minutes, e.g. index.html?simulate=fast&fast=3&drop=6 raises the fast-breathing
// alarm after 3 s and the connection-lost alarm after 6 s, with no board attached.
function applyUrlOverrides() {
  const params = new URLSearchParams(location.search);
  if (params.get('fast')) elements.fastLimit.value = params.get('fast');
  if (params.get('notAttached')) elements.notAttachedLimit.value = params.get('notAttached');
  if (params.get('stale')) elements.staleLimit.value = params.get('stale');
  return params;
}

function main() {
  const reason = checkWebBluetooth();
  if (reason) {
    elements.unsupportedReason.textContent = ' ' + reason;
    elements.unsupported.classList.remove('hidden');
    setConnectionStatus('Web Bluetooth unavailable', 'offline');
  }
  elements.connectButton.addEventListener('click', connectBoard);
  elements.simulateButton.addEventListener('click', function () {
    if (simulateTimer) stopSimulation(false);
    else startSimulation();
  });
  elements.dropButton.addEventListener('click', function () {
    if (mode === 'sim') stopSimulation(true);
    else if (device) device.gatt.disconnect();
  });
  elements.silenceButton.addEventListener('click', silenceAlarms);
  elements.alarmSilence.addEventListener('click', silenceAlarms);
  window.addEventListener('resize', drawTimeline);
  setInterval(tick, TICK_MS);
  log('page ready');

  const params = applyUrlOverrides();
  const simulate = params.get('simulate');
  if (simulate) {
    // ?simulate=scenario cycles a mixed scenario; ?simulate=fast holds one state.
    pinnedState = simulate === 'scenario' ? null : normalizeState(simulate);
    startSimulation();
    const dropAfter = parseFloat(params.get('drop'));
    if (isFinite(dropAfter) && dropAfter > 0) {
      setTimeout(function () { stopSimulation(true); }, dropAfter * 1000);
    }
  }
}

main();
