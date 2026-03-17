#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <time.h>
#include <Preferences.h>
#include "WebInterface.h"
#include "config.h"

// =====================================================
// External variables from main.cpp
// =====================================================

extern NutrientPump pumps[];
extern const int PUMP_COUNT;

extern unsigned long pumpStopTimes[];
extern const unsigned long PUMP_AUTO_OFF_MS;

extern float readPH();
extern float readEC();
extern float readTemp();
extern float readHumidity();

extern float phHistory[];
extern float ecHistory[];
extern float tempHistory[];
extern float humidityHistory[];
extern float microHistory[];
extern float groHistory[];
extern float bloomHistory[];

extern int historyIndex;
extern int historyCount;
extern const int HISTORY_SIZE;
extern int currentWeek;
extern int weekOffset;

extern float phOffset, phMult, ecOffset, ecMult;

extern float pumpFlowRateMlPerSec;
extern float pumpTotalMl[];
extern float pumpWeekMl[];
extern Recipe weekRecipes[];
extern Preferences prefs;

// =====================================================
// Web Interface Setup
// =====================================================

void setupWebInterface(WebServer &server)
{
    // ---------------- Main Dashboard ----------------
    server.on("/", [&server]() {
server.send_P(200, "text/html", R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Hydroponics Controller</title>
<script src="https://cdn.jsdelivr.net/npm/chart.js@4.4.0/dist/chart.umd.min.js"></script>
<script src="https://cdn.jsdelivr.net/npm/chartjs-plugin-annotation@3.0.1/dist/chartjs-plugin-annotation.min.js"></script>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:system-ui,sans-serif;background:#111827;color:#e5e7eb;padding:16px}
h1{text-align:center;color:#34d399;margin-bottom:20px;font-size:22px;letter-spacing:.02em}
h2{font-size:14px;color:#34d399;margin:22px 0 10px;padding-bottom:6px;border-bottom:1px solid #1f2937;text-transform:uppercase;letter-spacing:.06em}
.cards{display:flex;gap:10px;flex-wrap:wrap;margin-bottom:4px}
.card{flex:1;min-width:100px;background:#1f2937;border-radius:12px;padding:14px 10px;text-align:center;border:1px solid #374151}
.card-label{font-size:10px;color:#6b7280;text-transform:uppercase;letter-spacing:.08em;margin-bottom:6px}
.card-value{font-size:32px;font-weight:700;color:#34d399;line-height:1.1}
.card-unit{font-size:10px;color:#4b5563;margin-top:3px}
.pump-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(145px,1fr));gap:10px}
.pump-card{background:#1f2937;border-radius:10px;padding:12px;border:2px solid #374151;transition:border-color .2s}
.pump-card.on{border-color:#34d399}
.pump-name{font-size:13px;font-weight:600;color:#d1d5db;margin-bottom:8px}
.pump-btns{display:flex;gap:6px}
.btn-on,.btn-off{flex:1;padding:7px 0;border:none;border-radius:6px;cursor:pointer;font-size:12px;font-weight:700;transition:opacity .15s}
.btn-on{background:#34d399;color:#111827}.btn-on:hover{opacity:.85}
.btn-off{background:#ef4444;color:#fff}.btn-off:hover{opacity:.85}
.pump-timer{font-size:11px;color:#34d399;margin-top:5px;min-height:15px}
.gtoggle{display:flex;gap:8px;margin-bottom:10px;flex-wrap:wrap;align-items:center}
.gtbtn{background:#1f2937;color:#9ca3af;border:1px solid #374151;border-radius:6px;padding:7px 18px;cursor:pointer;font-size:12px;font-weight:600;transition:all .15s}
.gtbtn.active{background:#34d399;color:#111827;border-color:#34d399}
.chart-box{background:#1f2937;border-radius:12px;padding:16px;margin-bottom:12px;border:1px solid #374151}
.chart-box h3{font-size:11px;color:#6b7280;margin-bottom:10px;text-transform:uppercase;letter-spacing:.06em}
canvas{max-height:220px}
.thresh-panel{background:#0d1117;border:1px solid #374151;border-radius:8px;padding:14px;margin-bottom:14px}
.thresh-row{display:flex;align-items:center;gap:8px;margin-bottom:8px;flex-wrap:wrap}
.thresh-row:last-child{margin-bottom:0}
.thresh-label{font-size:12px;color:#9ca3af;min-width:90px}
.thresh-input{background:#1f2937;border:1px solid #374151;border-radius:5px;color:#e5e7eb;padding:5px 8px;font-size:12px;width:72px}
.thresh-input:focus{outline:none;border-color:#34d399}
.thresh-sep{font-size:12px;color:#6b7280}
.status-bar{display:flex;gap:12px;flex-wrap:wrap;background:#0d1117;border:1px solid #374151;border-radius:8px;padding:10px 14px;margin-bottom:14px}
.status-item{font-size:11px;color:#6b7280}
.status-item span{color:#9ca3af;font-weight:600}
.week-ctrl{display:flex;align-items:center;gap:10px;margin-bottom:14px}
.week-btn{background:#1f2937;color:#34d399;border:1px solid #374151;border-radius:6px;padding:6px 14px;cursor:pointer;font-size:18px;font-weight:700}
.week-btn:hover{border-color:#34d399}
.week-display{font-size:15px;font-weight:700;color:#e5e7eb;min-width:70px;text-align:center}
.cal-panel{background:#0d1117;border:1px solid #374151;border-radius:8px;padding:14px;margin-bottom:14px}
.cal-row{display:flex;align-items:center;gap:8px;margin-bottom:8px;flex-wrap:wrap}
.cal-row:last-child{margin-bottom:0}
.cal-label{font-size:12px;color:#9ca3af;min-width:40px}
.cal-input{background:#1f2937;border:1px solid #374151;border-radius:5px;color:#e5e7eb;padding:5px 8px;font-size:12px;width:72px}
.cal-input:focus{outline:none;border-color:#34d399}
.cal-btn{background:#374151;color:#e5e7eb;border:1px solid #4b5563;border-radius:5px;padding:5px 12px;font-size:12px;cursor:pointer}
.cal-btn:hover{background:#34d399;color:#111827;border-color:#34d399}
.card.alert{border:2px solid #ef4444 !important;background:#2d1515}
.card.alert .card-value{color:#ef4444}
.recipe-panel{background:#0d1117;border:1px solid #374151;border-radius:8px;padding:14px;margin-bottom:14px}
.recipe-header{display:flex;justify-content:space-between;align-items:center;flex-wrap:wrap;gap:8px;margin-bottom:12px}
.recipe-header span:first-child{font-size:13px;font-weight:700;color:#34d399}
.recipe-flow{font-size:12px;color:#9ca3af;display:flex;align-items:center;gap:6px}
.usage-table{width:100%;border-collapse:collapse;font-size:12px}
.usage-table th{color:#6b7280;text-align:left;padding:4px 8px;border-bottom:1px solid #374151;font-weight:600}
.usage-table td{color:#e5e7eb;padding:5px 8px;border-bottom:1px solid #1f2937}
.usage-table tr:last-child td{border-bottom:none}
.reset-row{display:flex;gap:8px;margin-top:12px;flex-wrap:wrap}
</style>
</head>
<body>
<h1>&#127807; Hydroponics Controller</h1>

<div class="status-bar">
  <div class="status-item">Uptime: <span id="st-uptime">--</span></div>
  <div class="status-item">WiFi: <span id="st-rssi">--</span> dBm</div>
  <div class="status-item">Week: <span id="st-week">--</span></div>
  <div class="status-item">Free Heap: <span id="st-heap">--</span> B</div>
</div>

<div class="cards">
  <div class="card">
    <div class="card-label">pH</div>
    <div class="card-value" id="v-ph">--</div>
  </div>
  <div class="card">
    <div class="card-label">TDS</div>
    <div class="card-value" id="v-ec">--</div>
    <div class="card-unit">ppm</div>
  </div>
  <div class="card">
    <div class="card-label">Temperature</div>
    <div class="card-value" id="v-temp">--</div>
    <div class="card-unit">&deg;F</div>
  </div>
  <div class="card">
    <div class="card-label">Humidity</div>
    <div class="card-value" id="v-hum">--</div>
    <div class="card-unit">%RH</div>
  </div>
</div>

<h2>Growth Week</h2>
<div class="week-ctrl">
  <button class="week-btn" onclick="changeWeek(-1)">&#8722;</button>
  <span class="week-display" id="week-display">Week --</span>
  <button class="week-btn" onclick="changeWeek(+1)">&#43;</button>
</div>

<h2>Recipe &amp; Usage</h2>
<div class="recipe-panel">
  <div class="recipe-header">
    <span id="recipe-week-label">Week -- Recipe</span>
    <span class="recipe-flow">Flow: <input class="cal-input" id="flow-input" type="number" step="0.1" min="0.1" value="1.0"> ml/s <button class="cal-btn" onclick="setFlowRate()">Set</button></span>
  </div>
  <table class="usage-table">
    <thead><tr><th>Nutrient</th><th>Target</th><th>This Week</th><th>All Time</th></tr></thead>
    <tbody id="usage-body"></tbody>
  </table>
  <div class="reset-row">
    <button class="cal-btn" onclick="resetMl('week')">Reset This Week</button>
    <button class="cal-btn" onclick="resetMl('total')">Reset All Time</button>
    <button class="cal-btn" style="border-color:#ef4444;color:#ef4444" onclick="newBatch()">&#127807; New Batch</button>
  </div>
</div>

<h2>Pump Controls</h2>
<div class="pump-grid" id="pump-grid"></div>

<h2>Sensor Calibration</h2>
<div class="cal-panel">
  <div class="cal-row">
    <span class="cal-label">pH</span>
    Offset <input class="cal-input" type="number" id="cal-ph-offset" step="0.01" value="0">
    &times; Mult <input class="cal-input" type="number" id="cal-ph-mult" step="0.01" value="1">
    <button class="cal-btn" onclick="applyCal('ph')">Apply</button>
  </div>
  <div class="cal-row">
    <span class="cal-label">EC</span>
    Offset <input class="cal-input" type="number" id="cal-ec-offset" step="0.01" value="0">
    &times; Mult <input class="cal-input" type="number" id="cal-ec-mult" step="0.01" value="1">
    <button class="cal-btn" onclick="applyCal('ec')">Apply</button>
  </div>
</div>

<h2>Sensor History</h2>
<div class="gtoggle">
  <button class="gtbtn active" id="btn-c"      onclick="setView('c')">Combined</button>
  <button class="gtbtn"        id="btn-s"      onclick="setView('s')">Separate</button>
  <button class="gtbtn"        id="btn-thresh" onclick="toggleThresholds()">Thresholds: OFF</button>
</div>

<div id="thresh-panel" style="display:none">
  <div class="thresh-row">
    <span class="thresh-label">pH</span>
    <input class="thresh-input" type="number" id="th-ph-min"   value="5"   step="0.1" onchange="updateThreshold('ph')">
    <span class="thresh-sep">to</span>
    <input class="thresh-input" type="number" id="th-ph-max"   value="9"   step="0.1" onchange="updateThreshold('ph')">
  </div>
  <div class="thresh-row">
    <span class="thresh-label">TDS (ppm)</span>
    <input class="thresh-input" type="number" id="th-ec-min"   value="130" step="1"   onchange="updateThreshold('ec')">
    <span class="thresh-sep">to</span>
    <input class="thresh-input" type="number" id="th-ec-max"   value="150" step="1"   onchange="updateThreshold('ec')">
  </div>
  <div class="thresh-row">
    <span class="thresh-label">Temp (&deg;F)</span>
    <input class="thresh-input" type="number" id="th-temp-min" value="60"  step="0.5" onchange="updateThreshold('temp')">
    <span class="thresh-sep">to</span>
    <input class="thresh-input" type="number" id="th-temp-max" value="80"  step="0.5" onchange="updateThreshold('temp')">
  </div>
  <div class="thresh-row">
    <span class="thresh-label">Humidity (%)</span>
    <input class="thresh-input" type="number" id="th-hum-min"  value="40"  step="1"   onchange="updateThreshold('hum')">
    <span class="thresh-sep">to</span>
    <input class="thresh-input" type="number" id="th-hum-max"  value="80"  step="1"   onchange="updateThreshold('hum')">
  </div>
</div>

<div id="view-c">
  <div class="chart-box">
    <h3>All Sensors</h3>
    <canvas id="ch-all"></canvas>
  </div>
</div>
<div id="view-s" style="display:none">
  <div class="chart-box"><h3>pH</h3><canvas id="ch-ph"></canvas></div>
  <div class="chart-box"><h3>TDS (ppm)</h3><canvas id="ch-ec"></canvas></div>
  <div class="chart-box"><h3>Temperature (&deg;F)</h3><canvas id="ch-temp"></canvas></div>
  <div class="chart-box"><h3>Humidity (%RH)</h3><canvas id="ch-hum"></canvas></div>
</div>

<script>
const AUTO_OFF_MS = 30000;
let pumpStopAt     = {};
let histData       = null;
let viewMode       = 'c';
let charts         = {};
let showThresholds = false;

const thresholds = {
  ph:   { min: 5.0,  max: 9.0  },
  ec:   { min: 130,  max: 150  },
  temp: { min: 60.0, max: 80.0 },
  hum:  { min: 40.0, max: 80.0 }
};

const COLORS = { ph:'#34d399', ec:'#fbbf24', temp:'#f87171', hum:'#a78bfa' };

// °C -> °F conversion for history arrays
const toFArr = (arr) => arr.map(c => parseFloat((c * 9 / 5 + 32).toFixed(2)));

// ---- Threshold annotations ----
function mkAnnotations(key) {
  if (!showThresholds) return {};
  const t = thresholds[key];
  const mkLine = (val) => ({
    type: 'line', yMin: val, yMax: val,
    borderColor: '#fbbf24', borderWidth: 1.5, borderDash: [6, 3]
  });
  return { minLine: mkLine(t.min), maxLine: mkLine(t.max) };
}

// ---- Chart options ----
function baseOpts(yLabel, annotKey) {
  return {
    responsive: true,
    maintainAspectRatio: true,
    animation: false,
    interaction: { mode:'index', intersect:false },
    plugins: {
      legend: { labels: { color:'#9ca3af', boxWidth:12, font:{size:11} } },
      tooltip: {
        mode: 'index', intersect: false,
        backgroundColor: '#111827', borderColor: '#374151', borderWidth: 1,
        titleColor: '#9ca3af', bodyColor: '#e5e7eb', padding: 10,
        callbacks: {
          title: (items) => items[0].label,
          label: (item)  => ' ' + item.dataset.label + ': ' + item.formattedValue
        }
      },
      annotation: { annotations: annotKey ? mkAnnotations(annotKey) : {} }
    },
    scales: {
      x: { ticks:{ color:'#6b7280', font:{size:10}, maxTicksLimit:10 }, grid:{ color:'#1f2937' } },
      y: {
        min: 0,
        ticks: { color:'#9ca3af', font:{size:11} },
        grid:  { color:'#1f2937' },
        title: { display: !!yLabel, text: yLabel||'', color:'#6b7280', font:{size:10} }
      }
    }
  };
}

function mkLabels(n) {
  return Array.from({length:n}, (_,i) => {
    const s = (n - 1 - i) * 15;
    if (s === 0)  return 'now';
    if (s < 60)   return '-' + s + 's';
    if (s < 3600) return '-' + Math.round(s/60) + 'm';
    return '-' + (s/3600).toFixed(1) + 'h';
  });
}

function destroyChart(id) { if (charts[id]) { charts[id].destroy(); delete charts[id]; } }

function ds(label, data, color) {
  return { label, data, borderColor:color, backgroundColor:color+'22',
           tension:.3, pointRadius:2, pointHoverRadius:6, fill:true, borderWidth:2 };
}

// ---- Sensors ----
async function refreshSensors() {
  try {
    const d = await fetch('/sensors').then(r => r.json());
    document.getElementById('v-ph').textContent   = d.ph.toFixed(2);
    document.getElementById('v-ec').textContent   = d.ec.toFixed(2);
    document.getElementById('v-temp').textContent = (d.temp * 9/5 + 32).toFixed(1);
    document.getElementById('v-hum').textContent  = d.humidity >= 0 ? d.humidity.toFixed(1) : '--';
    const tempF = d.temp * 9/5 + 32;
    const checks = [
      { id:'v-ph',   val:d.ph,       key:'ph'   },
      { id:'v-ec',   val:d.ec,       key:'ec'   },
      { id:'v-temp', val:tempF,      key:'temp' },
      { id:'v-hum',  val:d.humidity, key:'hum'  }
    ];
    for (const c of checks) {
      const card = document.getElementById(c.id).closest('.card');
      const t = thresholds[c.key];
      card.classList.toggle('alert', c.val < t.min || c.val > t.max);
    }
  } catch(e) {}
}
setInterval(refreshSensors, 5000);
refreshSensors();

// ---- Pumps ----
async function refreshPumps() {
  try {
    const d = await fetch('/pumps').then(r => r.json());
    const grid = document.getElementById('pump-grid');
    grid.innerHTML = '';
    for (const [name, info] of Object.entries(d)) {
      if (info.state && info.remainingMs > 0) pumpStopAt[name] = Date.now() + info.remainingMs;
      else if (!info.state) delete pumpStopAt[name];
      const card = document.createElement('div');
      card.className = 'pump-card' + (info.state ? ' on' : '');
      card.innerHTML =
        '<div class="pump-name">' + name + '</div>' +
        '<div class="pump-btns">' +
          '<button class="btn-on"  data-n="' + name + '" data-a="on"  onclick="dose(this)">ON</button>' +
          '<button class="btn-off" data-n="' + name + '" data-a="off" onclick="dose(this)">OFF</button>' +
        '</div>' +
        '<div class="pump-timer" id="t-' + name.replace(/ /g,'_') + '"></div>';
      grid.appendChild(card);
    }
  } catch(e) {}
}

async function dose(btn) {
  const name = btn.dataset.n, action = btn.dataset.a;
  await fetch('/dose?id=' + name.toLowerCase().replace(/ /g,'') + '&action=' + action);
  if (action === 'on') pumpStopAt[name] = Date.now() + AUTO_OFF_MS;
  else delete pumpStopAt[name];
  refreshPumps();
}

function tickTimers() {
  const now = Date.now();
  for (const [name, stopAt] of Object.entries(pumpStopAt)) {
    const el = document.getElementById('t-' + name.replace(/ /g,'_'));
    if (!el) continue;
    const rem = stopAt - now;
    if (rem > 0) { el.textContent = 'Auto-off: ' + Math.ceil(rem/1000) + 's'; }
    else { el.textContent = ''; delete pumpStopAt[name]; refreshPumps(); }
  }
}
setInterval(tickTimers, 500);
setInterval(refreshPumps, 10000);
refreshPumps();

// ---- Charts ----
function renderCombined(d) {
  destroyChart('all');
  const ctx = document.getElementById('ch-all').getContext('2d');
  charts['all'] = new Chart(ctx, {
    type: 'line',
    data: {
      labels: mkLabels(d.ph.length),
      datasets: [
        ds('pH',       d.ph,       COLORS.ph),
        ds('TDS',      d.ec,       COLORS.ec),
        ds('Temp(F)',  d.temp,     COLORS.temp),
        ds('Humidity', d.humidity, COLORS.hum)
      ]
    },
    options: baseOpts(null, null)
  });
}

function renderSeparate(d) {
  const sets = [
    { id:'ph',   label:'pH',        data:d.ph,       color:COLORS.ph,   canvas:'ch-ph',   ak:'ph'   },
    { id:'ec',   label:'TDS (ppm)', data:d.ec,       color:COLORS.ec,   canvas:'ch-ec',   ak:'ec'   },
    { id:'temp', label:'Temp (F)',  data:d.temp,     color:COLORS.temp, canvas:'ch-temp', ak:'temp' },
    { id:'hum',  label:'Humidity',  data:d.humidity, color:COLORS.hum,  canvas:'ch-hum',  ak:'hum'  }
  ];
  for (const s of sets) {
    destroyChart(s.id);
    const ctx = document.getElementById(s.canvas).getContext('2d');
    charts[s.id] = new Chart(ctx, {
      type: 'line',
      data: { labels: mkLabels(s.data.length), datasets: [ds(s.label, s.data, s.color)] },
      options: baseOpts(s.label, s.ak)
    });
  }
}

async function loadHistory() {
  try {
    const raw   = await fetch('/history').then(r => r.json());
    const count = Math.min(raw.count || 0, raw.ph.length);
    const tail  = (arr) => arr.slice(arr.length - count);
    histData = {
      ph:          tail(raw.ph),
      ec:          tail(raw.ec),
      temp:        toFArr(tail(raw.temp)),   // convert °C -> °F
      humidity:    tail(raw.humidity),
      currentWeek: raw.currentWeek
    };
    document.getElementById('week-display').textContent = 'Week ' + raw.currentWeek;
    if (viewMode === 'c') renderCombined(histData);
    else                  renderSeparate(histData);
  } catch(e) {}
}

function setView(v) {
  viewMode = v;
  document.getElementById('btn-c').classList.toggle('active', v === 'c');
  document.getElementById('btn-s').classList.toggle('active', v === 's');
  document.getElementById('view-c').style.display = v === 'c' ? 'block' : 'none';
  document.getElementById('view-s').style.display = v === 's' ? 'block' : 'none';
  if (histData) { if (v === 'c') renderCombined(histData); else renderSeparate(histData); }
}

// ---- Thresholds ----
function toggleThresholds() {
  showThresholds = !showThresholds;
  const btn = document.getElementById('btn-thresh');
  btn.classList.toggle('active', showThresholds);
  btn.textContent = 'Thresholds: ' + (showThresholds ? 'ON' : 'OFF');
  document.getElementById('thresh-panel').style.display = showThresholds ? 'block' : 'none';
  if (histData) { if (viewMode === 'c') renderCombined(histData); else renderSeparate(histData); }
}

function updateThreshold(key) {
  thresholds[key].min = parseFloat(document.getElementById('th-' + key + '-min').value);
  thresholds[key].max = parseFloat(document.getElementById('th-' + key + '-max').value);
  if (showThresholds && histData) {
    if (viewMode === 'c') renderCombined(histData); else renderSeparate(histData);
  }
}

// ---- Recipe & Usage ----
async function refreshRecipe() {
  try {
    const d = await fetch('/recipe').then(r => r.json());
    document.getElementById('recipe-week-label').textContent = 'Week ' + d.week + ' Recipe';
    document.getElementById('flow-input').value = d.flowRate.toFixed(2);
    const rows = [
      { name:'Micro',   target: d.recipe.micro  },
      { name:'Gro',     target: d.recipe.gro    },
      { name:'Bloom',   target: d.recipe.bloom  },
      { name:'pH Up',   target: d.recipe.phUp   },
      { name:'pH Down', target: d.recipe.phDown }
    ];
    const tbody = document.getElementById('usage-body');
    tbody.innerHTML = '';
    for (const r of rows) {
      const wk  = (d.weekMl[r.name]  || 0).toFixed(1);
      const tot = (d.totalMl[r.name] || 0).toFixed(1);
      tbody.innerHTML += '<tr><td>' + r.name + '</td><td>' + r.target + '</td><td>' + wk + ' ml</td><td>' + tot + ' ml</td></tr>';
    }
  } catch(e) {}
}
async function setFlowRate() {
  const r = parseFloat(document.getElementById('flow-input').value) || 1.0;
  await fetch('/flowrate?rate=' + r);
}
async function resetMl(which) {
  await fetch('/resetml?which=' + which);
  refreshRecipe();
}
async function newBatch() {
  if (!confirm('Start a new batch? This will reset all ml totals and set the week back to 1.')) return;
  await fetch('/resetml?which=total');
  await fetch('/setweek?week=1');
  loadHistory();
  refreshRecipe();
}
setInterval(refreshRecipe, 15000);
refreshRecipe();

// ---- Status bar ----
async function refreshStatus() {
  try {
    const d = await fetch('/status').then(r => r.json());
    const s = d.uptimeSec, h = Math.floor(s/3600), m = Math.floor((s%3600)/60), sec = s%60;
    const fmt = n => String(n).padStart(2,'0');
    document.getElementById('st-uptime').textContent = h+':'+fmt(m)+':'+fmt(sec);
    document.getElementById('st-rssi').textContent   = d.rssi;
    document.getElementById('st-week').textContent   = d.week;
    document.getElementById('st-heap').textContent   = d.freeHeap.toLocaleString();
  } catch(e) {}
}
setInterval(refreshStatus, 10000); refreshStatus();

// ---- Week control ----
async function changeWeek(delta) {
  const cur  = histData ? histData.currentWeek : 1;
  const next = Math.max(1, Math.min(12, cur + delta));
  await fetch('/setweek?week=' + next);
  loadHistory();
}

// ---- Calibration ----
async function loadCalibration() {
  try {
    const d = await fetch('/calibration').then(r => r.json());
    document.getElementById('cal-ph-offset').value = d.ph.offset;
    document.getElementById('cal-ph-mult').value   = d.ph.mult;
    document.getElementById('cal-ec-offset').value = d.ec.offset;
    document.getElementById('cal-ec-mult').value   = d.ec.mult;
  } catch(e) {}
}
async function applyCal(sensor) {
  const offset = parseFloat(document.getElementById('cal-'+sensor+'-offset').value)||0;
  const mult   = parseFloat(document.getElementById('cal-'+sensor+'-mult').value)||1;
  await fetch('/calibrate?sensor='+sensor+'&offset='+offset+'&mult='+mult);
}
loadCalibration();

setInterval(loadHistory, 15000);
loadHistory();
</script>
</body>
</html>
)HTML");
    });

    // ---------------- Current Sensors (JSON) ----------------
    server.on("/sensors", [&server]() {
JsonDocument doc;
        doc["ph"]       = readPH();
        doc["ec"]       = readEC();
        doc["temp"]     = readTemp();
        doc["humidity"] = readHumidity();
        String output;
        serializeJson(doc, output);
        server.send(200, "application/json", output);
    });

    // ---------------- Pump Status + Remaining Time ----------------
    server.on("/pumps", [&server]() {
JsonDocument doc;
        unsigned long now = millis();

        for (int i = 0; i < PUMP_COUNT; i++) {
            JsonObject p = doc[pumps[i].name].to<JsonObject>();
            p["state"] = pumps[i].state;
            unsigned long rem = (pumps[i].state && pumpStopTimes[i] > now)
                                ? (pumpStopTimes[i] - now) : 0;
            p["remainingMs"] = rem;
        }

        String output;
        serializeJson(doc, output);
        server.send(200, "application/json", output);
    });

    // ---------------- Pump Control ----------------
    server.on("/dose", [&server]() {
if (!server.hasArg("id") || !server.hasArg("action")) {
            server.send(400, "text/plain", "Missing id/action");
            return;
        }

        String id = server.arg("id");
        id.toLowerCase();
        id.replace(" ", "");

        String action = server.arg("action");
        int index = -1;

        for (int i = 0; i < PUMP_COUNT; i++) {
            String pn = String(pumps[i].name);
            pn.toLowerCase();
            pn.replace(" ", "");
            if (pn == id) { index = i; break; }
        }

        if (index < 0) {
            server.send(404, "text/plain", "Pump not found");
            return;
        }

        if (action == "on") {
            // Enforce single-pump rule: stop any running pump (recording its ml) before starting this one
            for (int i = 0; i < PUMP_COUNT; i++) {
                if (i != index && pumps[i].state) recordPumpStop(i);
            }
            pumps[index].state = true;
            digitalWrite(pumps[index].pin, HIGH);
            pumpStopTimes[index] = millis() + PUMP_AUTO_OFF_MS;
            pumps[index].onStartMillis = millis();
        } else {
            recordPumpStop(index);
        }

        server.send(200, "text/plain", "OK");
    });

    // ---------------- History ----------------
    server.on("/history", [&server]() {
        JsonDocument doc;

        JsonArray phArr       = doc["ph"].to<JsonArray>();
        JsonArray ecArr       = doc["ec"].to<JsonArray>();
        JsonArray tempArr     = doc["temp"].to<JsonArray>();
        JsonArray humidityArr = doc["humidity"].to<JsonArray>();
        JsonArray microArr    = doc["micro"].to<JsonArray>();
        JsonArray groArr      = doc["gro"].to<JsonArray>();
        JsonArray bloomArr    = doc["bloom"].to<JsonArray>();

        for (int i = 0; i < HISTORY_SIZE; i++) {
            int idx = (historyIndex + i) % HISTORY_SIZE;
            phArr.add(phHistory[idx]);
            ecArr.add(ecHistory[idx]);
            tempArr.add(tempHistory[idx]);
            humidityArr.add(humidityHistory[idx]);
            microArr.add(microHistory[idx]);
            groArr.add(groHistory[idx]);
            bloomArr.add(bloomHistory[idx]);
        }

        doc["currentWeek"] = currentWeek;
        doc["count"]       = historyCount;

        String output;
        serializeJson(doc, output);
        server.send(200, "application/json", output);
    });

    // ---------------- System Status ----------------
    server.on("/status", [&server]() {
JsonDocument doc;
        doc["uptimeSec"] = millis() / 1000UL;
        doc["rssi"]      = WiFi.RSSI();
        doc["freeHeap"]  = ESP.getFreeHeap();
        doc["week"]      = currentWeek;
        String output; serializeJson(doc, output);
        server.send(200, "application/json", output);
    });

    // ---------------- Set Week ----------------
    server.on("/setweek", [&server]() {
if (!server.hasArg("week")) { server.send(400, "text/plain", "Missing week"); return; }
        int w = server.arg("week").toInt();
        if (w < 1 || w > 12) { server.send(400, "text/plain", "Out of range 1-12"); return; }
        time_t now_t = time(nullptr);
        int ntpWeek = (int)((now_t - (time_t)GROW_START_EPOCH) / (7L * 86400L)) + 1;
        weekOffset = w - ntpWeek;
        currentWeek = w;
        server.send(200, "text/plain", "OK");
    });

    // ---------------- Calibration GET ----------------
    server.on("/calibration", [&server]() {
        JsonDocument doc;
        doc["ph"]["offset"] = phOffset; doc["ph"]["mult"] = phMult;
        doc["ec"]["offset"] = ecOffset; doc["ec"]["mult"] = ecMult;
        String out; serializeJson(doc, out);
        server.send(200, "application/json", out);
    });

    // ---------------- Calibration SET ----------------
    server.on("/calibrate", [&server]() {
        if (!server.hasArg("sensor")) { server.send(400, "text/plain", "Missing sensor"); return; }
        String s  = server.arg("sensor");
        float off = server.hasArg("offset") ? server.arg("offset").toFloat() : 0.0f;
        float mul = server.hasArg("mult")   ? server.arg("mult").toFloat()   : 1.0f;
        if (mul == 0.0f) mul = 1.0f;
        if      (s == "ph") { phOffset = off; phMult = mul; }
        else if (s == "ec") { ecOffset = off; ecMult = mul; }
        else { server.send(400, "text/plain", "Unknown sensor, use ph or ec"); return; }
        server.send(200, "text/plain", "OK");
    });

    // ---------------- Recipe & Usage ----------------
    server.on("/recipe", [&server]() {
        JsonDocument doc;
        int week = constrain(currentWeek, 1, 12);
        Recipe r = weekRecipes[week - 1];
        doc["week"]              = week;
        doc["recipe"]["micro"]   = r.micro;
        doc["recipe"]["gro"]     = r.gro;
        doc["recipe"]["bloom"]   = r.bloom;
        doc["recipe"]["phUp"]    = r.phUp;
        doc["recipe"]["phDown"]  = r.phDown;
        doc["flowRate"]          = pumpFlowRateMlPerSec;
        JsonObject week_ml  = doc["weekMl"].to<JsonObject>();
        JsonObject total_ml = doc["totalMl"].to<JsonObject>();
        for (int i = 0; i < PUMP_COUNT; i++) {
            week_ml[pumps[i].name]  = pumpWeekMl[i];
            total_ml[pumps[i].name] = pumpTotalMl[i];
        }
        String out; serializeJson(doc, out);
        server.send(200, "application/json", out);
    });

    // ---------------- Set Flow Rate ----------------
    server.on("/flowrate", [&server]() {
        if (!server.hasArg("rate")) { server.send(400, "text/plain", "Missing rate"); return; }
        float r = server.arg("rate").toFloat();
        if (r <= 0) { server.send(400, "text/plain", "Rate must be > 0"); return; }
        pumpFlowRateMlPerSec = r;
        prefs.begin("hydro", false);
        prefs.putFloat("flow_rate", r);
        prefs.end();
        server.send(200, "text/plain", "OK");
    });

    // ---------------- Reset ML Totals ----------------
    server.on("/resetml", [&server]() {
        String which = server.hasArg("which") ? server.arg("which") : "week";
        prefs.begin("hydro", false);
        for (int i = 0; i < PUMP_COUNT; i++) {
            if (which == "total") { pumpTotalMl[i] = 0; prefs.putFloat(("tot_"+String(i)).c_str(), 0.0f); }
            pumpWeekMl[i] = 0; prefs.putFloat(("wk_"+String(i)).c_str(), 0.0f);
        }
        prefs.end();
        server.send(200, "text/plain", "OK");
    });
}
