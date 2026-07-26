/** =================================================================
 * chaos STATION — app.js
 * 4 Themes, TFT Remote, SVG Pomodoro, CSS Gauges, API syncing
 * ================================================================= */

// =================================================================
// ---- State & Config
// =================================================================
let chart;
let historyData = { labels: [], tempBME: [] };
let isPageVisible = true;

// Poll intervals
let dataPollTimer = null;
let statusPollTimer = null;

// Settings state
let currentThemeIndex = 0; // 0=Nord,1=Cyber,2=Coder,3=Gruv
let currentTFTPage = 0;
let pageEnabled = [true,true,true,true,true,false,false,false,false,false];
let todoCount = 0;
let pomodoroActive = false;
let pomodoroRemSec = 0;
let localPomodoroTimer = null;

// TFT Page Info
const TFT_PAGES = [
  { id:0, icon:'📊', name:'Dash' },
  { id:1, icon:'🌡️', name:'Temp' },
  { id:2, icon:'💧', name:'Atmos' },
  { id:3, icon:'📝', name:'Notes' },
  { id:4, icon:'✅', name:'Tasks' },
  { id:5, icon:'⛅', name:'F-cast' },
  { id:6, icon:'🕐', name:'Clock' },
  { id:7, icon:'💻', name:'Sys' },
  { id:8, icon:'🍅', name:'Pomo' },
  { id:9, icon:'📈', name:'Health' }
];

// =================================================================
// ---- DOM Elements
// =================================================================
const el = {
  connPill: document.getElementById('conn-pill'),
  connDot: document.getElementById('conn-dot'),
  connLabel: document.getElementById('conn-label'),
  clock: document.getElementById('live-clock'),
  
  // Gauges
  dhtTemp: document.getElementById('dht-temp'),
  dhtHum: document.getElementById('dht-hum'),
  bmpTemp: document.getElementById('bmp-temp'),
  bmpPress: document.getElementById('bmp-press'),
  wxCondition: document.getElementById('wx-condition'),
  
  // Rings
  rDhtTemp: document.getElementById('ring-dht-temp'),
  rDhtHum: document.getElementById('ring-dht-hum'),
  rBmpTemp: document.getElementById('ring-bmp-temp'),
  rBmpPress: document.getElementById('ring-bmp-press'),
  
  // System
  uptime: document.getElementById('sys-uptime'),
  heap: document.getElementById('sys-heap'),
  rssi: document.getElementById('sys-rssi'),
  lastUpdate: document.getElementById('last-update'),
  refreshBtn: document.getElementById('refresh-btn'),
  
  // Settings & TFT
  pageGrid: document.getElementById('page-grid'),
  pagesGridSet: document.getElementById('pages-grid'),
  activePageNum: document.getElementById('active-page-num'),
  tftCarousel: document.getElementById('tftCarousel'),
  speedRange: document.getElementById('speed-range'),
  speedVal: document.getElementById('speed-val'),
  oledBtns: document.getElementById('oled-mode-btns'),
  notesFontSize: document.getElementById('notesFontSize'),
  todoFontSize: document.getElementById('todoFontSize'),
  saveSettingsBtn: document.getElementById('save-settings-btn'),
  saveTftBtn: document.getElementById('save-tft-btn'),
  settingsStatus: document.getElementById('settings-status'),
  tftSettingsStatus: document.getElementById('tft-settings-status'),
  
  // Themes
  themeCards: document.querySelectorAll('.theme-card'),
  
  // Notes / Tasks
  notesInput: document.getElementById('notes-input'),
  notesChars: document.getElementById('notes-chars'),
  saveNotesBtn: document.getElementById('save-notes-btn'),
  notesStatus: document.getElementById('notes-status'),
  todoInput: document.getElementById('todo-input'),
  addTodoBtn: document.getElementById('add-todo-btn'),
  todoList: document.getElementById('todo-list'),
  todoBadge: document.getElementById('todo-badge'),
  
  // Pomodoro
  pomoArc: document.getElementById('pomo-arc'),
  pomoTime: document.getElementById('pomo-time'),
  pomoState: document.getElementById('pomo-state-label'),
  pomoDur: document.getElementById('pomodoro-duration'),
  btnStartPomo: document.getElementById('start-pomodoro-btn'),
  btnStopPomo: document.getElementById('stop-pomodoro-btn'),
  pomoStatus: document.getElementById('pomodoro-status')
};

// =================================================================
// ---- Utility
// =================================================================
function escapeHTML(str) {
  if (!str) return '';
  return str.replace(/[&<>'"]/g, tag => ({
    '&': '&amp;', '<': '&lt;', '>': '&gt;', "'": '&#39;', '"': '&quot;'
  }[tag]));
}

function showStatus(elem, msg, isError = false) {
  if (!elem) return;
  elem.textContent = msg;
  elem.className = 'status-msg ' + (isError ? 'error' : '');
  setTimeout(() => { elem.textContent = ''; }, 3000);
}

function setConnection(online) {
  el.connDot.className = 'conn-dot ' + (online ? 'online' : 'offline');
  el.connLabel.textContent = online ? 'Online' : 'Offline';
}

function formatUptime(secs) {
  const h = Math.floor(secs / 3600);
  const m = Math.floor((secs % 3600) / 60);
  return `${h}h ${m}m`;
}

function getThemeAccentColor() {
  const t = document.body.className;
  if (t.includes('cyberpunk')) return '#FF00FF';
  if (t.includes('coder')) return '#00FF41';
  if (t.includes('gruvbox')) return '#D79921';
  return '#88C0D0'; // Nord
}

function updateGaugeRing(ringEl, val, min, max, baseColor) {
  if (!ringEl) return;
  let pct = (val - min) / (max - min) * 100;
  pct = Math.max(0, Math.min(100, pct));
  const t = document.body.className;
  let trackColor = 'rgba(30,41,59,0.9)'; // nord
  if (t.includes('cyberpunk')) trackColor = 'rgba(26,26,46,0.9)';
  if (t.includes('coder')) trackColor = 'rgba(0,32,0,0.8)';
  if (t.includes('gruvbox')) trackColor = 'rgba(40,40,40,0.9)';
  
  ringEl.style.background = `conic-gradient(${baseColor} ${pct}%, ${trackColor} ${pct}%)`;
}

// =================================================================
// ---- Chart.js
// =================================================================
function initChart() {
  const ctx = document.getElementById('tempChart').getContext('2d');
  const accent = getThemeAccentColor();
  
  Chart.defaults.color = '#81A1C1';
  Chart.defaults.font.family = "'Outfit', sans-serif";
  
  chart = new Chart(ctx, {
    type: 'line',
    data: {
      labels: historyData.labels,
      datasets: [{
        label: 'Outdoor Temp (°C)',
        data: historyData.tempBME,
        borderColor: accent,
        backgroundColor: 'rgba(0,0,0,0)', // gradient applied dynamically later if needed
        borderWidth: 2,
        pointRadius: 1,
        pointHoverRadius: 5,
        tension: 0.3,
        fill: false
      }]
    },
    options: {
      responsive: true,
      maintainAspectRatio: false,
      plugins: {
        legend: { display: false },
        tooltip: { mode: 'index', intersect: false }
      },
      scales: {
        x: { grid: { color: 'rgba(255,255,255,0.05)' } },
        y: { 
          grid: { color: 'rgba(255,255,255,0.05)' },
          title: { display: true, text: 'Temp °C' }
        }
      },
      animation: false
    }
  });
}

function updateChartColors() {
  if (!chart) return;
  const accent = getThemeAccentColor();
  const t = document.body.className;
  let gridCol = 'rgba(255,255,255,0.05)';
  let textCol = '#81A1C1'; // nord
  
  if (t.includes('cyberpunk')) { gridCol = 'rgba(255,0,255,0.1)'; textCol = '#AACCEE'; }
  if (t.includes('coder')) { gridCol = 'rgba(0,255,65,0.1)'; textCol = '#005500'; }
  if (t.includes('gruvbox')) { gridCol = 'rgba(215,153,33,0.1)'; textCol = '#928374'; }
  
  chart.data.datasets[0].borderColor = accent;
  chart.options.scales.x.grid.color = gridCol;
  chart.options.scales.y.grid.color = gridCol;
  Chart.defaults.color = textCol;
  if(t.includes('coder')) Chart.defaults.font.family = "'Courier New', monospace";
  else Chart.defaults.font.family = "'Outfit', sans-serif";
  chart.update();
}

async function fetchHistory() {
  try {
    const res = await fetch('/api/history');
    if (!res.ok) return;
    const csv = await res.text();
    const lines = csv.trim().split('\n');
    historyData.labels = []; historyData.tempBME = [];
    
    // Skip header line
    for (let i = 1; i < lines.length; i++) {
      const parts = lines[i].split(',');
      if (parts.length >= 5) {
        const time = parts[0].split(' ')[1].substring(0, 5); // HH:MM
        const bmpT = parseFloat(parts[3]);
        if (!isNaN(bmpT)) {
          historyData.labels.push(time);
          historyData.tempBME.push(bmpT);
        }
      }
    }
    
    if (chart) {
      chart.data.labels = historyData.labels;
      chart.data.datasets[0].data = historyData.tempBME;
      chart.update();
    }
  } catch (e) {
    console.error("History fetch error:", e);
  }
}

// =================================================================
// ---- Local Clock
// =================================================================
setInterval(() => {
  const now = new Date();
  el.clock.textContent = now.toLocaleTimeString('en-US', { hour12: false });
}, 1000);

// =================================================================
// ---- Data Fetching (/api/data)
// =================================================================
async function fetchSensorData() {
  if (!isPageVisible) return;
  try {
    const res = await fetch('/api/data', { signal: AbortSignal.timeout(4000) });
    if (!res.ok) throw new Error('Bad response');
    const d = await res.json();
    
    setConnection(true);
    
    const dhtT = parseFloat(d.tempDHT).toFixed(1);
    const dhtH = parseFloat(d.humDHT).toFixed(0);
    const bmpT = parseFloat(d.tempBME).toFixed(1);
    const bmpP = parseFloat(d.pressBME).toFixed(0);
    
    // Values
    el.dhtTemp.textContent = isNaN(d.tempDHT) ? '--' : dhtT;
    el.dhtHum.textContent  = isNaN(d.humDHT)  ? '--' : dhtH;
    el.bmpTemp.textContent = isNaN(d.tempBME) ? '--' : bmpT;
    el.bmpPress.textContent= isNaN(d.pressBME)? '--' : (bmpP / 100).toFixed(0);
    
    // Error states (NaN)
    el.dhtTemp.classList.toggle('error-state', isNaN(d.tempDHT));
    el.bmpTemp.classList.toggle('error-state', isNaN(d.tempBME));
    
    // Rings
    const ac = getThemeAccentColor();
    const rd = document.body.className.includes('cyberpunk') ? '#FF0055' : 
               (document.body.className.includes('coder') ? '#FF3333' : 
               (document.body.className.includes('gruvbox') ? '#CC241D' : '#BF616A'));
               
    const bl = document.body.className.includes('cyberpunk') ? '#00FFFF' : 
               (document.body.className.includes('coder') ? '#33FF33' : 
               (document.body.className.includes('gruvbox') ? '#FE8019' : '#5E81AC'));
    
    updateGaugeRing(el.rDhtTemp, d.tempDHT, 0, 50, rd);
    updateGaugeRing(el.rDhtHum, d.humDHT, 0, 100, ac);
    updateGaugeRing(el.rBmpTemp, d.tempBME, -10, 50, rd);
    updateGaugeRing(el.rBmpPress, d.pressBME/100, 900, 1100, bl);
    
    // System
    el.uptime.textContent = formatUptime(d.uptime || 0);
    el.heap.textContent = (d.freeHeap / 1024).toFixed(0) + ' KB';
    el.rssi.textContent = d.rssi;
    el.lastUpdate.textContent = new Date().toLocaleTimeString();
    
    // Weather condition basic heuristic
    const ph = d.pressBME / 100;
    if (ph < 1000) el.wxCondition.textContent = "🌧️ Rainy / Storm";
    else if (ph > 1020) el.wxCondition.textContent = "☀️ Clear / Fair";
    else el.wxCondition.textContent = "⛅ Changing";
    
  } catch(e) {
    console.error("Data fetch error", e);
    setConnection(false);
  }
}

// =================================================================
// ---- Status Fetching (/api/status) -> TFT Syncing
// =================================================================
async function fetchStatus() {
  if (!isPageVisible) return;
  try {
    const res = await fetch('/api/status', { signal: AbortSignal.timeout(3000) });
    if (!res.ok) return;
    const s = await res.json();
    
    // Sync TFT Active Page
    if (s.page !== undefined && s.page !== currentTFTPage) {
      currentTFTPage = s.page;
      updateTFTPageUI();
    }
    
    // Sync Theme (if ESP32 changed it via config or reset)
    if (s.theme !== undefined && s.theme !== currentThemeIndex) {
      currentThemeIndex = s.theme;
      applyThemeClass(currentThemeIndex);
    }
    
    // Sync Pomodoro
    if (s.pomodoroActive !== undefined) {
      pomodoroActive = s.pomodoroActive;
      pomodoroRemSec = s.pomodoroRemSec;
      updatePomodoroUI();
      if (pomodoroActive && !localPomodoroTimer) {
        startLocalPomodoroTick();
      } else if (!pomodoroActive && localPomodoroTimer) {
        clearInterval(localPomodoroTimer);
        localPomodoroTimer = null;
      }
    }
    
  } catch(e) {
    // Ignore status fetch errors silently
  }
}

// =================================================================
// ---- TFT Page UI (Remote)
// =================================================================
function renderTFTPages() {
  // Remote Grid
  el.pageGrid.innerHTML = '';
  // Settings Grid
  el.pagesGridSet.innerHTML = '';
  
  TFT_PAGES.forEach((p, i) => {
    // 1. Remote Button
    const btn = document.createElement('button');
    btn.className = 'page-btn' + (i === currentTFTPage ? ' active' : '') + (!pageEnabled[i] ? ' disabled' : '');
    btn.innerHTML = `<span class="p-icon">${p.icon}</span><span class="p-name">${p.name}</span>`;
    btn.onclick = () => jumpToTFTPage(i);
    el.pageGrid.appendChild(btn);
    
    // 2. Settings Toggle
    const tog = document.createElement('div');
    tog.className = 'page-toggle';
    tog.innerHTML = `
      <div class="page-toggle-left">
        <span class="page-toggle-icon">${p.icon}</span>
        <span class="page-toggle-name">${p.name}</span>
      </div>
      <label class="pill-toggle">
        <input type="checkbox" data-page="${i}" ${pageEnabled[i]?'checked':''}>
        <span class="pill-slider"></span>
      </label>
    `;
    const cb = tog.querySelector('input');
    cb.addEventListener('change', (e) => {
      pageEnabled[i] = e.target.checked;
      updateTFTPageUI();
    });
    // Click row to toggle
    tog.addEventListener('click', (e) => {
      if(e.target === cb || e.target.classList.contains('pill-slider')) return;
      cb.checked = !cb.checked;
      pageEnabled[i] = cb.checked;
      updateTFTPageUI();
    });
    
    el.pagesGridSet.appendChild(tog);
  });
  
  el.activePageNum.textContent = currentTFTPage;
}

function updateTFTPageUI() {
  // Update active state and disabled state on remote buttons
  Array.from(el.pageGrid.children).forEach((btn, i) => {
    if (i === currentTFTPage) btn.classList.add('active');
    else btn.classList.remove('active');
    
    if (pageEnabled[i]) btn.classList.remove('disabled');
    else btn.classList.add('disabled');
  });
  
  el.activePageNum.textContent = currentTFTPage;
  
  // Update settings toggles
  Array.from(el.pagesGridSet.children).forEach((tog, i) => {
    tog.querySelector('input').checked = pageEnabled[i];
  });
}

async function jumpToTFTPage(idx) {
  if (!pageEnabled[idx]) {
    alert("This page is disabled in settings.");
    return;
  }
  try {
    const res = await fetch('/api/page', {
      method: 'POST',
      headers: {'Content-Type':'application/json'},
      body: JSON.stringify({ page: idx })
    });
    if (res.ok) {
      currentTFTPage = idx;
      updateTFTPageUI();
    }
  } catch(e) {
    console.error("Jump page error", e);
  }
}

// =================================================================
// ---- Themes
// =================================================================
function applyThemeClass(idx) {
  const themes = ['theme-nord', 'theme-cyberpunk', 'theme-coder', 'theme-gruvbox'];
  document.body.className = themes[idx] || themes[0];
  
  // Update Theme Cards
  el.themeCards.forEach(c => {
    if (parseInt(c.dataset.tft) === idx) c.classList.add('selected');
    else c.classList.remove('selected');
  });
  
  updateChartColors();
  fetchSensorData(); // Force re-render of gauge colors
}

el.themeCards.forEach(card => {
  card.addEventListener('click', async () => {
    const idx = parseInt(card.dataset.tft);
    applyThemeClass(idx);
    currentThemeIndex = idx;
    
    // Auto-save theme via settings API
    await submitSettings();
  });
});

// =================================================================
// ---- Pomodoro SVG & Logic
// =================================================================
function startLocalPomodoroTick() {
  if (localPomodoroTimer) clearInterval(localPomodoroTimer);
  localPomodoroTimer = setInterval(() => {
    if (pomodoroRemSec > 0) pomodoroRemSec--;
    updatePomodoroUI();
    if (pomodoroRemSec <= 0) clearInterval(localPomodoroTimer);
  }, 1000);
}

function updatePomodoroUI() {
  if (!pomodoroActive) {
    el.pomoTime.textContent = '25:00';
    el.pomoState.textContent = 'READY';
    el.pomoArc.style.strokeDashoffset = 314;
    el.pomoArc.style.stroke = 'var(--accent)';
    return;
  }
  
  if (pomodoroRemSec <= 0) {
    el.pomoTime.textContent = '00:00';
    el.pomoState.textContent = 'DONE!';
    el.pomoArc.style.strokeDashoffset = 0;
    el.pomoArc.style.stroke = 'var(--success)';
    return;
  }
  
  const m = Math.floor(pomodoroRemSec / 60);
  const s = pomodoroRemSec % 60;
  el.pomoTime.textContent = `${m.toString().padStart(2,'0')}:${s.toString().padStart(2,'0')}`;
  el.pomoState.textContent = 'FOCUS';
  
  // Total duration approx from duration input or 25m fallback
  // Actually we don't know total duration easily here, assume 25 for visual ring
  // A better way is fetchStatus returning total, but we'll approximate 25m max
  const maxSec = parseInt(el.pomoDur.value || 25) * 60;
  let pct = pomodoroRemSec / maxSec;
  pct = Math.max(0, Math.min(1, pct));
  
  const offset = 314 - (314 * pct);
  el.pomoArc.style.strokeDashoffset = offset;
  
  let col = 'var(--success)';
  if (pct < 0.2) col = 'var(--danger)';
  else if (pct < 0.5) col = 'var(--warn)';
  
  el.pomoArc.style.stroke = col;
}

el.btnStartPomo.addEventListener('click', async () => {
  const mins = parseInt(el.pomoDur.value || 25);
  try {
    const res = await fetch('/api/pomodoro', {
      method: 'POST',
      headers: {'Content-Type':'application/json'},
      body: JSON.stringify({ action: 'start', duration: mins })
    });
    if (res.ok) {
      pomodoroActive = true;
      pomodoroRemSec = mins * 60;
      updatePomodoroUI();
      startLocalPomodoroTick();
      showStatus(el.pomoStatus, 'Started!');
    } else showStatus(el.pomoStatus, 'Error', true);
  } catch(e) {
    showStatus(el.pomoStatus, 'Network error', true);
  }
});

el.btnStopPomo.addEventListener('click', async () => {
  try {
    const res = await fetch('/api/pomodoro', {
      method: 'POST',
      headers: {'Content-Type':'application/json'},
      body: JSON.stringify({ action: 'stop' })
    });
    if (res.ok) {
      pomodoroActive = false;
      if (localPomodoroTimer) clearInterval(localPomodoroTimer);
      updatePomodoroUI();
      showStatus(el.pomoStatus, 'Stopped.');
    } else showStatus(el.pomoStatus, 'Error', true);
  } catch(e) {
    showStatus(el.pomoStatus, 'Network error', true);
  }
});

// =================================================================
// ---- Settings Load / Save
// =================================================================
async function loadSettings() {
  try {
    const res = await fetch('/api/settings');
    if (!res.ok) return;
    const d = await res.json();
    
    // OLED Mode
    Array.from(el.oledBtns.children).forEach(b => b.classList.remove('active'));
    const obtn = document.querySelector(`.mode-btn[data-mode="${d.oledMode}"]`);
    if(obtn) obtn.classList.add('active');
    
    // Theme
    if (d.tftTheme !== undefined) {
      currentThemeIndex = d.tftTheme;
      applyThemeClass(currentThemeIndex);
    }
    
    // TFT Carousel
    if(d.tftCarousel !== undefined) el.tftCarousel.checked = d.tftCarousel;
    if(d.tftSpeed) {
      el.speedRange.value = d.tftSpeed;
      el.speedVal.textContent = (d.tftSpeed / 1000) + 's';
    }
    
    // Pages
    if(d.pages && d.pages.length === 10) {
      pageEnabled = d.pages;
    }
    
    // Fonts
    if(d.notesFontSize) el.notesFontSize.value = d.notesFontSize;
    if(d.todoFontSize) el.todoFontSize.value = d.todoFontSize;
    
    renderTFTPages();
  } catch(e) {
    console.error("Load settings error", e);
  }
}

async function submitSettings(statusElem = el.settingsStatus) {
  const oledActive = document.querySelector('.mode-btn.active');
  const oledMode = oledActive ? parseInt(oledActive.dataset.mode) : 0;
  
  const payload = {
    oledMode: oledMode,
    tftTheme: currentThemeIndex,
    tftCarousel: el.tftCarousel.checked,
    tftSpeed: parseInt(el.speedRange.value),
    pages: pageEnabled,
    notesFontSize: parseInt(el.notesFontSize.value),
    todoFontSize: parseInt(el.todoFontSize.value)
  };
  
  try {
    const res = await fetch('/api/settings', {
      method: 'POST',
      headers: {'Content-Type':'application/json'},
      body: JSON.stringify(payload)
    });
    if (res.ok) showStatus(statusElem, 'Settings applied & saved!');
    else showStatus(statusElem, 'Error saving', true);
  } catch(e) {
    showStatus(statusElem, 'Network error', true);
  }
}

// OLED Buttons logic
Array.from(el.oledBtns.children).forEach(btn => {
  btn.addEventListener('click', () => {
    Array.from(el.oledBtns.children).forEach(b => b.classList.remove('active'));
    btn.classList.add('active');
  });
});

// Speed slider logic
el.speedRange.addEventListener('input', (e) => {
  el.speedVal.textContent = (e.target.value / 1000) + 's';
});

el.saveSettingsBtn.addEventListener('click', () => submitSettings(el.settingsStatus));
el.saveTftBtn.addEventListener('click', () => submitSettings(el.tftSettingsStatus));

// =================================================================
// ---- Notes & Tasks
// =================================================================
let todos = [];

function renderTodos() {
  el.todoList.innerHTML = '';
  todos.forEach((t, i) => {
    const li = document.createElement('li');
    li.className = 'todo-item';
    li.innerHTML = `
      <span>${escapeHTML(t)}</span>
      <button class="delete-todo-btn" onclick="removeTodo(${i})" title="Remove task">✖</button>
    `;
    el.todoList.appendChild(li);
  });
  el.todoBadge.textContent = `${todos.length}/5`;
}

window.removeTodo = function(idx) {
  todos.splice(idx, 1);
  renderTodos();
};

el.addTodoBtn.addEventListener('click', () => {
  const t = el.todoInput.value.trim();
  if (t && todos.length < 5) {
    todos.push(t.substring(0,25));
    el.todoInput.value = '';
    renderTodos();
  }
});

el.notesInput.addEventListener('input', (e) => {
  el.notesChars.textContent = e.target.value.length;
});

async function loadNotes() {
  try {
    const res = await fetch('/api/notes');
    if (!res.ok) return;
    const d = await res.json();
    el.notesInput.value = d.notes || '';
    el.notesChars.textContent = el.notesInput.value.length;
    todos = d.todos || [];
    renderTodos();
  } catch(e) {
    console.error("Load notes error", e);
  }
}

el.saveNotesBtn.addEventListener('click', async () => {
  try {
    const res = await fetch('/api/notes', {
      method: 'POST',
      headers: {'Content-Type':'application/json'},
      body: JSON.stringify({
        notes: el.notesInput.value,
        todos: todos
      })
    });
    if (res.ok) showStatus(el.notesStatus, 'Notes saved!');
    else showStatus(el.notesStatus, 'Error saving notes', true);
  } catch(e) {
    showStatus(el.notesStatus, 'Network error', true);
  }
});

// =================================================================
// ---- Init & Loops
// =================================================================
document.addEventListener('visibilitychange', () => {
  isPageVisible = !document.hidden;
  if (isPageVisible) {
    fetchSensorData();
    fetchStatus();
  }
});

el.refreshBtn.addEventListener('click', () => {
  fetchSensorData();
  fetchStatus();
  fetchHistory();
});

// Boot
applyThemeClass(0); // Set default while loading
renderTFTPages();
initChart();

// =================================================================
// ---- Wi-Fi Provisioning
// =================================================================
async function scanWifiNetworks() {
  const btn = document.getElementById('scan-wifi-btn');
  const select = document.getElementById('wifi-ssids');
  const status = document.getElementById('wifi-status');
  if (!select) return;
  
  if (btn) btn.disabled = true;
  select.innerHTML = '<option value="">Scanning for networks...</option>';
  
  try {
    const res = await fetch('/api/wifi/scan');
    if (res.ok) {
      const networks = await res.json();
      select.innerHTML = '';
      if (networks.length === 0) {
        select.innerHTML = '<option value="">No networks found</option>';
      } else {
        networks.forEach(net => {
          const opt = document.createElement('option');
          opt.value = net.ssid;
          opt.textContent = `${net.ssid} (${net.rssi} dBm) ${net.secure ? '🔒' : '🔓'}`;
          select.appendChild(opt);
        });
      }
      showStatus(status, `Found ${networks.length} networks`);
    } else {
      select.innerHTML = '<option value="">Failed to scan</option>';
      showStatus(status, 'Scan failed', true);
    }
  } catch(e) {
    select.innerHTML = '<option value="">Scan error</option>';
    showStatus(status, 'Network error', true);
  } finally {
    if (btn) btn.disabled = false;
  }
}

async function saveWifiCredentials() {
  const select = document.getElementById('wifi-ssids');
  const passInput = document.getElementById('wifi-pass');
  const status = document.getElementById('wifi-status');
  
  const ssid = select ? select.value : '';
  const pass = passInput ? passInput.value : '';
  
  if (!ssid) {
    showStatus(status, 'Please select or enter an SSID', true);
    return;
  }
  
  showStatus(status, 'Saving & connecting...');
  try {
    const res = await fetch('/api/wifi/save', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ ssid, pass })
    });
    const data = await res.json();
    if (res.ok) {
      showStatus(status, data.message || 'Saved! Reconnecting...');
    } else {
      showStatus(status, data.message || 'Failed to save', true);
    }
  } catch(e) {
    showStatus(status, 'Network error', true);
  }
}

async function resetWifiCredentials() {
  const status = document.getElementById('wifi-status');
  if (!confirm('Are you sure you want to forget Wi-Fi credentials and return to Hotspot Mode?')) return;
  
  try {
    const res = await fetch('/api/wifi/reset', { method: 'POST' });
    const data = await res.json();
    if (res.ok) {
      showStatus(status, data.message || 'Reset complete! Rebooting...');
    } else {
      showStatus(status, 'Reset failed', true);
    }
  } catch(e) {
    showStatus(status, 'Network error', true);
  }
}

// Bind Wi-Fi buttons
const scanBtn = document.getElementById('scan-wifi-btn');
const saveWBtn = document.getElementById('save-wifi-btn');
const resetWBtn = document.getElementById('reset-wifi-btn');
if (scanBtn) scanBtn.addEventListener('click', scanWifiNetworks);
if (saveWBtn) saveWBtn.addEventListener('click', saveWifiCredentials);
if (resetWBtn) resetWBtn.addEventListener('click', resetWifiCredentials);

// Sequence
(async function init() {
  await loadSettings();
  await loadNotes();
  await fetchHistory();
  await fetchSensorData();
  await fetchStatus();
  
  // Loops
  dataPollTimer = setInterval(fetchSensorData, 2000); // 2s data
  statusPollTimer = setInterval(fetchStatus, 3000);   // 3s status sync
})();
