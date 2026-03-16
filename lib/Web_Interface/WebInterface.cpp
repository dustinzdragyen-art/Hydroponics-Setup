#include <Arduino.h>
#include <ArduinoJson.h>
#include "WebInterface.h"

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
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:system-ui,sans-serif;background:#111827;color:#e5e7eb;padding:16px}
h1{text-align:center;color:#34d399;margin-bottom:20px;font-size:22px;letter-spacing:.02em}
h2{font-size:14px;color:#34d399;margin:22px 0 10px;padding-bottom:6px;border-bottom:1px solid #1f2937;text-transform:uppercase;letter-spacing:.06em}

/* Sensor cards */
.cards{display:flex;gap:10px;flex-wrap:wrap;margin-bottom:4px}
.card{flex:1;min-width:100px;background:#1f2937;border-radius:12px;padding:14px 10px;text-align:center;border:1px solid #374151}
.card-label{font-size:10px;color:#6b7280;text-transform:uppercase;letter-spacing:.08em;margin-bottom:6px}
.card-value{font-size:32px;font-weight:700;color:#34d399;line-height:1.1}
.card-unit{font-size:10px;color:#4b5563;margin-top:3px}

/* Pump grid */
.pump-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(145px,1fr));gap:10px}
.pump-card{background:#1f2937;border-radius:10px;padding:12px;border:2px solid #374151;transition:border-color .2s}
.pump-card.on{border-color:#34d399}
.pump-name{font-size:13px;font-weight:600;color:#d1d5db;margin-bottom:8px}
.pump-btns{display:flex;gap:6px}
.btn-on,.btn-off{flex:1;padding:7px 0;border:none;border-radius:6px;cursor:pointer;font-size:12px;font-weight:700;transition:opacity .15s}
.btn-on{background:#34d399;color:#111827}.btn-on:hover{opacity:.85}
.btn-off{background:#ef4444;color:#fff}.btn-off:hover{opacity:.85}
.pump-timer{font-size:11px;color:#34d399;margin-top:5px;min-height:15px}

/* Graph section */
.gtoggle{display:flex;gap:8px;margin-bottom:14px;flex-wrap:wrap}
.gtbtn{background:#1f2937;color:#9ca3af;border:1px solid #374151;border-radius:6px;padding:7px 18px;cursor:pointer;font-size:12px;font-weight:600;transition:all .15s}
.gtbtn.active{background:#34d399;color:#111827;border-color:#34d399}
.chart-box{background:#1f2937;border-radius:12px;padding:16px;margin-bottom:12px;border:1px solid #374151}
.chart-box h3{font-size:11px;color:#6b7280;margin-bottom:10px;text-transform:uppercase;letter-spacing:.06em}
canvas{max-height:200px}
</style>
</head>
<body>
<h1>&#127807; Hydroponics Controller</h1>

<!-- Sensor Cards -->
<div class="cards">
  <div class="card">
    <div class="card-label">pH</div>
    <div class="card-value" id="v-ph">--</div>
  </div>
  <div class="card">
    <div class="card-label">TDS / EC</div>
    <div class="card-value" id="v-ec">--</div>
    <div class="card-unit">mS/cm</div>
  </div>
  <div class="card">
    <div class="card-label">Temperature</div>
    <div class="card-value" id="v-temp">--</div>
    <div class="card-unit">&deg;C</div>
  </div>
  <div class="card">
    <div class="card-label">Humidity</div>
    <div class="card-value" id="v-hum">--</div>
    <div class="card-unit">%RH</div>
  </div>
</div>

<!-- Pump Controls -->
<h2>Pump Controls</h2>
<div class="pump-grid" id="pump-grid"></div>

<!-- History Graphs -->
<h2>Sensor History</h2>
<div class="gtoggle">
  <button class="gtbtn active" id="btn-c" onclick="setView('c')">Combined</button>
  <button class="gtbtn"        id="btn-s" onclick="setView('s')">Separate</button>
</div>

<div id="view-c">
  <div class="chart-box">
    <h3>All Sensors &mdash; switch to Separate for individual scales</h3>
    <canvas id="ch-all"></canvas>
  </div>
</div>

<div id="view-s" style="display:none">
  <div class="chart-box"><h3>pH</h3><canvas id="ch-ph"></canvas></div>
  <div class="chart-box"><h3>TDS / EC (mS/cm)</h3><canvas id="ch-ec"></canvas></div>
  <div class="chart-box"><h3>Temperature (&deg;C)</h3><canvas id="ch-temp"></canvas></div>
  <div class="chart-box"><h3>Humidity (%RH)</h3><canvas id="ch-hum"></canvas></div>
</div>

<script>
const AUTO_OFF_MS = 30000;
let pumpStopAt = {};
let histData    = null;
let viewMode    = 'c';
let charts      = {};

// ---- Chart helpers ----
const COLORS = { ph:'#34d399', ec:'#fbbf24', temp:'#f87171', hum:'#a78bfa' };

function baseOpts(yLabel) {
  return {
    responsive: true,
    maintainAspectRatio: true,
    animation: false,
    interaction: { mode:'index', intersect:false },
    plugins: {
      legend: { labels: { color:'#9ca3af', boxWidth:12, font:{size:11} } },
      tooltip: {
        mode: 'index',
        intersect: false,
        backgroundColor: '#111827',
        borderColor: '#374151',
        borderWidth: 1,
        titleColor: '#9ca3af',
        bodyColor: '#e5e7eb',
        padding: 10,
        callbacks: {
          title: (items) => items[0].label,
          label: (item)  => ' ' + item.dataset.label + ': ' + item.formattedValue
        }
      }
    },
    scales: {
      x: { ticks:{ color:'#6b7280', font:{size:10}, maxTicksLimit:10 }, grid:{ color:'#1f2937' } },
      y: {
        ticks: { color:'#9ca3af', font:{size:11} },
        grid:  { color:'#1f2937' },
        title: { display: !!yLabel, text: yLabel||'', color:'#6b7280', font:{size:10} }
      }
    }
  };
}

function mkLabels(n) {
  return Array.from({length:n}, (_,i) => {
    const sAgo = (n - 1 - i) * 15;
    if (sAgo === 0)     return 'now';
    if (sAgo < 60)      return '-' + sAgo + 's';
    if (sAgo < 3600)    return '-' + Math.round(sAgo/60) + 'm';
    return '-' + (sAgo/3600).toFixed(1) + 'h';
  });
}

function destroyChart(id) { if (charts[id]) { charts[id].destroy(); delete charts[id]; } }

// ---- Sensors ----
async function refreshSensors() {
  try {
    const d = await fetch('/sensors').then(r => r.json());
    document.getElementById('v-ph').textContent   = d.ph.toFixed(2);
    document.getElementById('v-ec').textContent   = d.ec.toFixed(2);
    document.getElementById('v-temp').textContent = d.temp.toFixed(1);
    document.getElementById('v-hum').textContent  = d.humidity >= 0 ? d.humidity.toFixed(1) : '--';
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
      if (info.state && info.remainingMs > 0)
        pumpStopAt[name] = Date.now() + info.remainingMs;
      else if (!info.state)
        delete pumpStopAt[name];

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
  const name   = btn.dataset.n;
  const action = btn.dataset.a;
  const safe   = name.toLowerCase().replace(/ /g, '');
  await fetch('/dose?id=' + safe + '&action=' + action);
  if (action === 'on') pumpStopAt[name] = Date.now() + AUTO_OFF_MS;
  else delete pumpStopAt[name];
  refreshPumps();
}

function tickTimers() {
  const now = Date.now();
  for (const [name, stopAt] of Object.entries(pumpStopAt)) {
    const el  = document.getElementById('t-' + name.replace(/ /g, '_'));
    if (!el) continue;
    const rem = stopAt - now;
    if (rem > 0) {
      el.textContent = 'Auto-off: ' + Math.ceil(rem / 1000) + 's';
    } else {
      el.textContent = '';
      delete pumpStopAt[name];
      refreshPumps();
    }
  }
}

setInterval(tickTimers, 500);
setInterval(refreshPumps, 10000);
refreshPumps();

// ---- Charts ----
function ds(label, data, color) {
  return { label, data, borderColor:color, backgroundColor:color+'22',
           tension:.3, pointRadius:2, pointHoverRadius:6, fill:true, borderWidth:2 };
}

function renderCombined(d) {
  destroyChart('all');
  const ctx = document.getElementById('ch-all').getContext('2d');
  charts['all'] = new Chart(ctx, {
    type: 'line',
    data: {
      labels: mkLabels(d.ph.length),
      datasets: [
        ds('pH',      d.ph,       COLORS.ph),
        ds('EC',      d.ec,       COLORS.ec),
        ds('Temp',    d.temp,     COLORS.temp),
        ds('Humidity',d.humidity, COLORS.hum)
      ]
    },
    options: baseOpts()
  });
}

function renderSeparate(d) {
  const sets = [
    { id:'ph',   label:'pH',          data:d.ph,       color:COLORS.ph,   canvas:'ch-ph'   },
    { id:'ec',   label:'TDS / EC',    data:d.ec,       color:COLORS.ec,   canvas:'ch-ec'   },
    { id:'temp', label:'Temperature', data:d.temp,     color:COLORS.temp, canvas:'ch-temp' },
    { id:'hum',  label:'Humidity',    data:d.humidity, color:COLORS.hum,  canvas:'ch-hum'  }
  ];
  for (const s of sets) {
    destroyChart(s.id);
    const ctx = document.getElementById(s.canvas).getContext('2d');
    charts[s.id] = new Chart(ctx, {
      type: 'line',
      data: { labels: mkLabels(s.data.length), datasets: [ds(s.label, s.data, s.color)] },
      options: baseOpts(s.label)
    });
  }
}

async function loadHistory() {
  try {
    const raw = await fetch('/history').then(r => r.json());
    // Only slice the valid portion — avoids showing leading zeros before data starts
    const count = Math.min(raw.count || 0, raw.ph.length);
    const tail  = (arr) => arr.slice(arr.length - count);
    histData = {
      ph:       tail(raw.ph),
      ec:       tail(raw.ec),
      temp:     tail(raw.temp),
      humidity: tail(raw.humidity),
      currentWeek: raw.currentWeek
    };
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
  if (histData) {
    if (v === 'c') renderCombined(histData);
    else           renderSeparate(histData);
  }
}

setInterval(loadHistory, 15000); // match the 15s record interval
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
            // Enforce single-pump rule: turn off any running pump before starting this one
            for (int i = 0; i < PUMP_COUNT; i++) {
                if (i != index && pumps[i].state) {
                    pumps[i].state = false;
                    digitalWrite(pumps[i].pin, LOW);
                    pumpStopTimes[i] = 0;
                    pumps[i].onStartMillis = 0;
                }
            }
            pumps[index].state = true;
            digitalWrite(pumps[index].pin, HIGH);
            pumpStopTimes[index] = millis() + PUMP_AUTO_OFF_MS;
            pumps[index].onStartMillis = millis();
        } else {
            pumps[index].state = false;
            digitalWrite(pumps[index].pin, LOW);
            pumpStopTimes[index] = 0;
            pumps[index].onStartMillis = 0;
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
}
