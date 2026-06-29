#include "webserver.h"
#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "storage.h"

static AsyncWebServer server(80);
static SystemContext* sysCtxPtr = nullptr;

// ================================================================
// SESSION TOKENS — simple session management
// ================================================================
static String userSessionToken    = "";
static String teacherSessionToken = "";

static String generateToken() {
    String token = "";
    for (int i = 0; i < 16; i++) {
        token += String(random(0, 16), HEX);
    }
    return token;
}

static bool isUserLoggedIn(AsyncWebServerRequest* req) {
    if (!req->hasHeader("Cookie")) return false;
    String cookie = req->header("Cookie");
    return userSessionToken != "" && cookie.indexOf(userSessionToken) >= 0;
}

static bool isTeacherLoggedIn(AsyncWebServerRequest* req) {
    if (!req->hasHeader("Cookie")) return false;
    String cookie = req->header("Cookie");
    return teacherSessionToken != "" && cookie.indexOf(teacherSessionToken) >= 0;
}

// ================================================================
// DASHBOARD HTML
// ================================================================
static const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>AWO Dashboard</title>
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body { font-family: Arial, sans-serif; background: #1a1a2e; color: #eee; padding: 16px; }
        h1 { color: #e94560; margin-bottom: 16px; font-size: 1.4em; }
        h2 { font-size: 1em; color: #aaa; margin-bottom: 12px; }
        .grid { display: grid; grid-template-columns: 1fr 1fr; gap: 12px; margin-bottom: 16px; }
        .card { background: #16213e; border-radius: 10px; padding: 16px; }
        .card.full { grid-column: 1 / -1; }
        .value { font-size: 1.8em; font-weight: bold; color: #e94560; }
        .label { font-size: 0.8em; color: #aaa; margin-top: 4px; }
        .badge { display: inline-block; padding: 4px 10px; border-radius: 20px; 
                 font-size: 0.8em; font-weight: bold; }
        .badge-auto  { background: #1a472a; color: #69db7c; }
        .badge-manual{ background: #1a3a4a; color: #74c0fc; }
        .badge-lock  { background: #4a1942; color: #f783ac; }
        .badge-estop { background: #7a1f1f; color: #ff6b6b; }
        input[type=range] { width: 100%; margin: 8px 0; accent-color: #e94560; }
        input[type=password], input[type=text] { 
            width: 100%; padding: 10px; border-radius: 8px; 
            border: 1px solid #0f3460; background: #1a1a2e; 
            color: #eee; font-size: 0.9em; margin-bottom: 8px; }
        button { padding: 10px 20px; background: #e94560; color: white;
                 border: none; border-radius: 8px; cursor: pointer; 
                 font-size: 0.9em; width: 100%; margin-top: 8px; }
        button:hover { background: #c73652; }
        button.secondary { background: #0f3460; }
        button.secondary:hover { background: #1a4a80; }
        button.danger { background: #7a1f1f; }
        button.danger:hover { background: #9a2f2f; }
        .teacher-banner { background: #7a1f1f; color: #ff6b6b; padding: 12px; 
                          border-radius: 8px; text-align: center; 
                          margin-bottom: 16px; display: none; }
        .locked { opacity: 0.5; pointer-events: none; }
        canvas { max-height: 200px; }
        .tab-bar { display: flex; gap: 8px; margin-bottom: 16px; }
        .tab { padding: 8px 16px; border-radius: 8px; cursor: pointer; 
               background: #16213e; color: #aaa; font-size: 0.9em; }
        .tab.active { background: #e94560; color: white; }
        .tab-content { display: none; }
        .tab-content.active { display: block; }
        .msg { padding: 10px; border-radius: 8px; margin-top: 8px; 
               text-align: center; font-size: 0.85em; }
        .msg.ok  { background: #1a472a; color: #69db7c; }
        .msg.err { background: #4a1942; color: #f783ac; }
        .health-item { 
            display: flex; align-items: center; gap: 8px; 
            padding: 8px; border-radius: 8px; background: #1a1a2e;
            font-size: 0.85em;
        }
        .dot { 
            width: 10px; height: 10px; border-radius: 50%; 
            background: #69db7c; display: inline-block; flex-shrink: 0;
        }
        .dot.warn  { background: #ffd43b; }
        .dot.error { background: #ff6b6b; }
    </style>
</head>
<body>

<h1>🪟 AWO Dashboard</h1>

<!-- Teacher override banner -->
<div id="teacherBanner" class="teacher-banner">
    ⚠️ TEACHER OVERRIDE ACTIVE — All controls locked
</div>

<!-- Tab bar -->
<div class="tab-bar">
    <div class="tab active" onclick="showTab('overview')">Overview</div>
    <div class="tab" onclick="showTab('control')">Control</div>
    <div class="tab" onclick="showTab('settings')">Settings</div>
    <div class="tab" onclick="showTab('admin')">Admin</div>
</div>

<!-- OVERVIEW TAB -->
<div id="tab-overview" class="tab-content active">
    <div class="grid">
        <div class="card">
            <div class="value" id="indoorTemp">--</div>
            <div class="label">Indoor Temp (°C)</div>
        </div>
        <div class="card">
            <div class="value" id="indoorHumidity">--</div>
            <div class="label">Indoor Humidity (%)</div>
        </div>
        <div class="card">
            <div class="value" id="outdoorTemp">--</div>
            <div class="label">Outdoor Temp (°C)</div>
        </div>
        <div class="card">
            <div class="value" id="outdoorHumidity">--</div>
            <div class="label">Outdoor Humidity (%)</div>
        </div>
        <div class="card">
            <div class="value" id="windSpeed">--</div>
            <div class="label">Wind Speed (km/h)</div>
        </div>
        <div class="card">
            <div class="value" id="rainStatus">--</div>
            <div class="label">Rain Detected</div>
        </div>
        <div class="card">
            <div class="value" id="lightLevel">--</div>
            <div class="label">Light Level</div>
        </div>
        <div class="card">
            <div class="value" id="windowPos">--</div>
            <div class="label">Window Position (%)</div>
        </div>
        <div class="card full">
            <h2>System Status</h2>
            <span id="modeBadge" class="badge">--</span>
            &nbsp;
            <span id="stateBadge" class="badge">--</span>
        </div>
        <div class="card full">
            <h2>System Health</h2>
            <div id="healthGrid" style="display:grid; grid-template-columns: 1fr 1fr; gap:8px; margin-top:8px">
                <div class="health-item" id="health-estop">
                    <span class="dot"></span> Emergency Stop
                </div>
                <div class="health-item" id="health-outdoor">
                    <span class="dot"></span> Outdoor Module
                </div>
                <div class="health-item" id="health-overcurrent">
                    <span class="dot"></span> Motor Current
                </div>
                <div class="health-item" id="health-natural">
                    <span class="dot"></span> Natural Events
                </div>
            </div>
        </div>
        <div class="card full">
            <h2>Temperature History</h2>
            <canvas id="tempChart"></canvas>
        </div>
    </div>
</div>

<!-- CONTROL TAB -->
<div id="tab-control" class="tab-content">
    <div id="controlSection">
        <div id="loginSection" class="card" style="margin-bottom:12px">
            <h2>Login to Control</h2>
            <input type="password" id="userPass" placeholder="Enter password">
            <button onclick="userLogin()">Login</button>
            <div id="loginMsg" class="msg" style="display:none"></div>
        </div>
        <div id="controlPanel" style="display:none">
            <div class="card" style="margin-bottom:12px">
                <h2>Window Position</h2>
                <div class="value" id="posDisplay">--%</div>
                <input type="range" id="posSlider" min="0" max="100" step="5" value="0"
                       oninput="document.getElementById('posDisplay').textContent=this.value+'%'">
                <button onclick="setPosition()">Set Position</button>
            </div>
            <div class="card" style="margin-bottom:12px">
                <h2>Mode</h2>
                <button onclick="setMode('MANUAL')"   class="secondary">Manual</button>
                <button onclick="setMode('AUTOMATIC')" class="secondary">Automatic</button>
                <button onclick="setMode('LOCK')"     class="danger">Lock</button>
            </div>
            <div class="card">
                <h2>Temperature Settings</h2>
                <label>Target Temp (°C)</label>
                <input type="text" id="targetTemp" placeholder="e.g. 25">
                <label>Range ±(°C)</label>
                <input type="text" id="tempRange" placeholder="e.g. 2">
                <label>Wind Threshold (km/h)</label>
                <input type="text" id="windThresh" placeholder="e.g. 30">
                <button onclick="saveSettings()">Save Settings</button>
                <div id="settingsMsg" class="msg" style="display:none"></div>
            </div>
        </div>
    </div>
</div>

<!-- SETTINGS TAB -->
<div id="tab-settings" class="tab-content">
    <div class="card">
        <h2>Current Settings</h2>
        <p>Target Temp: <span id="setTempDisplay">--</span>°C</p>
        <br>
        <p>Temp Range: ±<span id="setRangeDisplay">--</span>°C</p>
        <br>
        <p>Wind Threshold: <span id="setWindDisplay">--</span> km/h</p>
    </div>
</div>

<!-- ADMIN TAB -->
<div id="tab-admin" class="tab-content">
    <div id="teacherLoginSection" class="card" style="margin-bottom:12px">
        <h2>Teacher Login</h2>
        <input type="password" id="teacherPass" placeholder="Teacher password">
        <button onclick="teacherLogin()">Login as Teacher</button>
        <div id="teacherLoginMsg" class="msg" style="display:none"></div>
    </div>
    <div id="teacherPanel" style="display:none">
        <div class="card" style="margin-bottom:12px">
            <h2>Teacher Override</h2>
            <p style="color:#aaa; margin-bottom:12px; font-size:0.85em">
                When enabled, all physical and web controls are locked.
            </p>
            <button id="overrideBtn" onclick="toggleOverride()">Enable Override</button>
        </div>
        <div class="card" style="margin-bottom:12px">
            <h2>Change User Password</h2>
            <input type="password" id="newUserPass" placeholder="New user password">
            <button onclick="changeUserPass()">Update</button>
            <div id="userPassMsg" class="msg" style="display:none"></div>
        </div>
        <div class="card">
            <h2>Change Teacher Password</h2>
            <input type="password" id="newTeacherPass" placeholder="New teacher password">
            <button onclick="changeTeacherPass()">Update</button>
            <div id="teacherPassMsg" class="msg" style="display:none"></div>
        </div>
        <div class="card" style="margin-top:12px">
            <h2>WiFi Settings</h2>
            <button class="danger" onclick="resetWiFi()">Reset WiFi (re-run setup)</button>
        </div>
    </div>
</div>

<script>
// ── CHART SETUP ─────────────────────────────────────────────────
const chartData = { labels: [], indoor: [], outdoor: [] };
const ctx = document.getElementById('tempChart').getContext('2d');
const tempChart = new Chart(ctx, {
    type: 'line',
    data: {
        labels: chartData.labels,
        datasets: [
            { label: 'Indoor', data: chartData.indoor, 
              borderColor: '#e94560', tension: 0.3, fill: false },
            { label: 'Outdoor', data: chartData.outdoor, 
              borderColor: '#74c0fc', tension: 0.3, fill: false }
        ]
    },
    options: {
        responsive: true,
        plugins: { legend: { labels: { color: '#eee' } } },
        scales: {
            x: { ticks: { color: '#aaa' }, grid: { color: '#333' } },
            y: { ticks: { color: '#aaa' }, grid: { color: '#333' } }
        }
    }
});

// ── TAB SWITCHING ────────────────────────────────────────────────
function showTab(name) {
    document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
    document.querySelectorAll('.tab-content').forEach(t => t.classList.remove('active'));
    document.getElementById('tab-' + name).classList.add('active');
    event.target.classList.add('active');
}

// ── DATA POLLING ─────────────────────────────────────────────────
let teacherOverrideActive = false;

function updateDashboard() {
    fetch('/api/data')
        .then(r => r.json())
        .then(d => {
            document.getElementById('indoorTemp').textContent     = d.indoorTemp.toFixed(1);
            document.getElementById('indoorHumidity').textContent = d.indoorHumidity.toFixed(1);
            document.getElementById('outdoorTemp').textContent    = d.outdoorTemp.toFixed(1);
            document.getElementById('outdoorHumidity').textContent= d.outdoorHumidity.toFixed(1);
            document.getElementById('windSpeed').textContent      = d.windSpeed.toFixed(1);
            document.getElementById('rainStatus').textContent     = d.rain ? '🌧 Yes' : '☀️ No';
            document.getElementById('lightLevel').textContent     = d.light.toFixed(0);
            document.getElementById('windowPos').textContent      = d.position + '%';
            document.getElementById('setTempDisplay').textContent = d.targetTemp.toFixed(1);
            document.getElementById('setRangeDisplay').textContent= d.tempRange.toFixed(1);
            document.getElementById('setWindDisplay').textContent = d.windThreshold.toFixed(1);

            // mode badge
            const modeBadge = document.getElementById('modeBadge');
            modeBadge.textContent = d.mode;
            modeBadge.className = 'badge badge-' + d.mode.toLowerCase();

            // state badge
            const stateBadge = document.getElementById('stateBadge');
            stateBadge.textContent = d.state;
            stateBadge.className = d.state === 'EMERGENCY_STOP' ? 
                'badge badge-estop' : 'badge badge-manual';

            // teacher override
            teacherOverrideActive = d.teacherOverride;
            document.getElementById('teacherBanner').style.display = 
                d.teacherOverride ? 'block' : 'none';
            document.getElementById('overrideBtn').textContent = 
                d.teacherOverride ? 'Disable Override' : 'Enable Override';

            // update chart (keep last 20 points)
            const now = new Date().toLocaleTimeString();
            if (chartData.labels.length > 20) {
                chartData.labels.shift();
                chartData.indoor.shift();
                chartData.outdoor.shift();
            }
            chartData.labels.push(now);
            chartData.indoor.push(d.indoorTemp);
            chartData.outdoor.push(d.outdoorTemp);
            tempChart.update();

            updateHealth(d); // update health indicators
        })
        .catch(() => console.log('Data fetch failed'));
}

// poll every 3 seconds
setInterval(updateDashboard, 3000);
updateDashboard();

// ── AUTH ─────────────────────────────────────────────────────────
function userLogin() {
    const pass = document.getElementById('userPass').value;
    fetch('/api/login', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: 'pass=' + encodeURIComponent(pass) + '&type=user'
    })
    .then(r => r.json())
    .then(d => {
        const msg = document.getElementById('loginMsg');
        msg.style.display = 'block';
        if (d.success) {
            msg.className = 'msg ok';
            msg.textContent = 'Logged in!';
            document.getElementById('loginSection').style.display  = 'none';
            document.getElementById('controlPanel').style.display  = 'block';
        } else {
            msg.className = 'msg err';
            msg.textContent = 'Wrong password';
        }
    });
}

function teacherLogin() {
    const pass = document.getElementById('teacherPass').value;
    fetch('/api/login', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: 'pass=' + encodeURIComponent(pass) + '&type=teacher'
    })
    .then(r => r.json())
    .then(d => {
        const msg = document.getElementById('teacherLoginMsg');
        msg.style.display = 'block';
        if (d.success) {
            msg.className = 'msg ok';
            msg.textContent = 'Teacher logged in!';
            document.getElementById('teacherLoginSection').style.display = 'none';
            document.getElementById('teacherPanel').style.display        = 'block';
        } else {
            msg.className = 'msg err';
            msg.textContent = 'Wrong password';
        }
    });
}

// ── CONTROLS ─────────────────────────────────────────────────────
function setPosition() {
    const pos = document.getElementById('posSlider').value;
    fetch('/api/position', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: 'pos=' + pos
    }).then(r => r.json()).then(d => alert(d.success ? 'Position set!' : 'Failed'));
}

function setMode(mode) {
    fetch('/api/mode', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: 'mode=' + mode
    }).then(r => r.json()).then(d => alert(d.success ? 'Mode set!' : 'Failed'));
}

function saveSettings() {
    const temp  = document.getElementById('targetTemp').value;
    const range = document.getElementById('tempRange').value;
    const wind  = document.getElementById('windThresh').value;
    fetch('/api/settings', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: `temp=${temp}&range=${range}&wind=${wind}`
    })
    .then(r => r.json())
    .then(d => {
        const msg = document.getElementById('settingsMsg');
        msg.style.display = 'block';
        msg.className = d.success ? 'msg ok' : 'msg err';
        msg.textContent = d.success ? 'Settings saved!' : 'Failed';
    });
}

function toggleOverride() {
    fetch('/api/override', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: 'enable=' + (!teacherOverrideActive ? '1' : '0')
    }).then(r => r.json()).then(d => {
        if (d.success) updateDashboard();
    });
}

function changeUserPass() {
    const pass = document.getElementById('newUserPass').value;
    fetch('/api/password', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: 'type=user&pass=' + encodeURIComponent(pass)
    })
    .then(r => r.json())
    .then(d => {
        const msg = document.getElementById('userPassMsg');
        msg.style.display = 'block';
        msg.className = d.success ? 'msg ok' : 'msg err';
        msg.textContent = d.success ? 'Password updated!' : 'Failed';
    });
}

function changeTeacherPass() {
    const pass = document.getElementById('newTeacherPass').value;
    fetch('/api/password', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: 'type=teacher&pass=' + encodeURIComponent(pass)
    })
    .then(r => r.json())
    .then(d => {
        const msg = document.getElementById('teacherPassMsg');
        msg.style.display = 'block';
        msg.className = d.success ? 'msg ok' : 'msg err';
        msg.textContent = d.success ? 'Password updated!' : 'Failed';
    });
}

function resetWiFi() {
    if (!confirm('Reset WiFi? Device will restart in setup mode.')) return;
    fetch('/api/resetwifi', { method: 'POST' })
        .then(() => alert('WiFi reset. Connect to AWO_Setup network.'));
}

// ── HEALTH STATUS UPDATES ─────────────────────────────────────
function updateHealth(data) {
    // emergency stop
    const estopDot = document.querySelector('#health-estop .dot');
    estopDot.className = data.estop ? 'dot error' : 'dot';
    document.querySelector('#health-estop').style.background = 
        data.estop ? '#4a1942' : '#1a1a2e';

    // outdoor module
    const outdoorDot = document.querySelector('#health-outdoor .dot');
    outdoorDot.className = data.outdoorStale ? 'dot error' : 'dot';
    document.querySelector('#health-outdoor').style.background = 
        data.outdoorStale ? '#4a1942' : '#1a1a2e';

    // motor overcurrent
    const ocDot = document.querySelector('#health-overcurrent .dot');
    ocDot.className = data.overcurrent ? 'dot warn' : 'dot';
    document.querySelector('#health-overcurrent').style.background = 
        data.overcurrent ? '#3a3a1a' : '#1a1a2e';

    // natural event
    const natDot = document.querySelector('#health-natural .dot');
    natDot.className = data.naturalEvent ? 'dot warn' : 'dot';
    document.querySelector('#health-natural').style.background = 
        data.naturalEvent ? '#3a3a1a' : '#1a1a2e';
}

</script>
</body>
</html>
)rawliteral";

// ================================================================
// INIT
// ================================================================
void webserver_init(SystemContext& ctx) {
    sysCtxPtr = &ctx;

    // ── GET / — serve dashboard ──────────────────────────────────
    server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send(200, "text/html", DASHBOARD_HTML);
    });

    // ── GET /api/data — return all data as JSON ──────────────────
    server.on("/api/data", HTTP_GET, [](AsyncWebServerRequest* req) {
        SystemContext& ctx = *sysCtxPtr;

        String stateStr, modeStr;
        switch (ctx.state) {
            case STATE_IDLE:           stateStr = "IDLE";           break;
            case STATE_ADJUSTING:      stateStr = "ADJUSTING";      break;
            case STATE_FORCE_CLOSING:  stateStr = "FORCE_CLOSING";  break;
            case STATE_EMERGENCY_STOP: stateStr = "EMERGENCY_STOP"; break;
            default:                   stateStr = "UNKNOWN";        break;
        }
        switch (ctx.mode) {
            case MODE_MANUAL:    modeStr = "MANUAL";    break;
            case MODE_AUTOMATIC: modeStr = "AUTOMATIC"; break;
            case MODE_LOCK:      modeStr = "LOCK";      break;
        }

        String json = "{";
        json += "\"indoorTemp\":"     + String(ctx.indoor.temperature, 1)  + ",";
        json += "\"indoorHumidity\":" + String(ctx.indoor.humidity, 1)     + ",";
        json += "\"outdoorTemp\":"    + String(ctx.outdoor.temperature, 1) + ",";
        json += "\"outdoorHumidity\":"+ String(ctx.outdoor.humidity, 1)    + ",";
        json += "\"windSpeed\":"      + String(ctx.outdoor.windSpeed, 1)   + ",";
        json += "\"rain\":"           + String(ctx.outdoor.rainDetected)   + ",";
        json += "\"light\":"          + String(ctx.outdoor.lightLevel, 1)  + ",";
        json += "\"position\":"       + String(ctx.currentPosition)        + ",";
        json += "\"targetTemp\":"     + String(ctx.setTargetTemp, 1)       + ",";
        json += "\"tempRange\":"      + String(ctx.setTempRange, 1)        + ",";
        json += "\"windThreshold\":"  + String(WIND_SPEED_MAX, 1)          + ",";
        json += "\"mode\":\""         + modeStr                            + "\",";
        json += "\"state\":\""        + stateStr                           + "\",";
        json += "\"estop\":"          + String(ctx.estopPressed)        + ",";
        json += "\"outdoorStale\":"   + String(ctx.outdoor.isStale)     + ",";
        json += "\"overcurrent\":"    + String(ctx.overcurrentDetected) + ",";
        json += "\"naturalEvent\":"   + String(ctx.naturalEventActive)  + ",";
        json += "\"teacherOverride\":" + String(ctx.teacherOverride)    ;
        json += "}";

        req->send(200, "application/json", json);
    });

    // ── POST /api/login ──────────────────────────────────────────
    server.on("/api/login", HTTP_POST, [](AsyncWebServerRequest* req) {
        SystemContext& ctx = *sysCtxPtr;
        String type = req->arg("type");
        String pass = req->arg("pass");

        bool success = false;
        String token = "";

        if (type == "user" && pass == String(ctx.userPassword)) {
            userSessionToken = generateToken();
            token   = userSessionToken;
            success = true;
        } else if (type == "teacher" && pass == String(ctx.teacherPassword)) {
            teacherSessionToken = generateToken();
            token   = teacherSessionToken;
            success = true;
        }

        AsyncWebServerResponse* response = req->beginResponse(200, 
            "application/json", 
            success ? "{\"success\":true}" : "{\"success\":false}");

        if (success) {
            response->addHeader("Set-Cookie", 
                "token=" + token + "; Path=/; HttpOnly");
        }
        req->send(response);
    });

    // ── POST /api/position ───────────────────────────────────────
    server.on("/api/position", HTTP_POST, [](AsyncWebServerRequest* req) {
        SystemContext& ctx = *sysCtxPtr;
        if (ctx.teacherOverride || !isUserLoggedIn(req)) {
            bool isTeacher = isTeacherLoggedIn(req);
            bool isUser    = isUserLoggedIn(req);

            if (ctx.teacherOverride && !isTeacher) {
                req->send(200, "application/json", "{\"success\":false}");
                return;
            }
            if (!isTeacher && !isUser) {
                req->send(200, "application/json", "{\"success\":false}");
                return;
            }
        }
        int pos = req->arg("pos").toInt();
        ctx.targetPosition      = constrain(pos, 0, 100);
        ctx.manualTargetPreview = ctx.targetPosition;
        ctx.windowConfirmed     = true;
        ctx.mode                = MODE_MANUAL;
        req->send(200, "application/json", "{\"success\":true}");
    });

    // ── POST /api/mode ───────────────────────────────────────────
server.on("/api/mode", HTTP_POST, [](AsyncWebServerRequest* req) {
    SystemContext& ctx = *sysCtxPtr;
    if (ctx.teacherOverride || !isUserLoggedIn(req)) {
        bool isTeacher = isTeacherLoggedIn(req);
        bool isUser    = isUserLoggedIn(req);

        if (ctx.teacherOverride && !isTeacher) {
            req->send(200, "application/json", "{\"success\":false}");
            return;
        }
        if (!isTeacher && !isUser) {
            req->send(200, "application/json", "{\"success\":false}");
            return;
        }
    }
    String mode = req->arg("mode");
    if (mode == "MANUAL") {
        ctx.mode                = MODE_MANUAL;
        ctx.uiState             = UI_MANUAL;        // ← update UI
        ctx.windowConfirmed     = true;
        ctx.targetPosition      = ctx.currentPosition;
        ctx.manualTargetPreview = ctx.currentPosition;
    } else if (mode == "AUTOMATIC") {
        ctx.mode          = MODE_AUTOMATIC;
        ctx.uiState       = UI_AUTOMATIC;           // ← update UI
        ctx.autoConfirmed = true;
    } else if (mode == "LOCK") {
        ctx.mode       = MODE_LOCK;
        ctx.uiState    = UI_LOCK;                   // ← update UI
        ctx.targetPosition = WINDOW_POS_MIN;
    }
    req->send(200, "application/json", "{\"success\":true}");
});

    // ── POST /api/settings ───────────────────────────────────────
server.on("/api/settings", HTTP_POST, [](AsyncWebServerRequest* req) {
    SystemContext& ctx = *sysCtxPtr;
    if (ctx.teacherOverride || !isUserLoggedIn(req)) {
        bool isTeacher = isTeacherLoggedIn(req);
        bool isUser    = isUserLoggedIn(req);

        if (ctx.teacherOverride && !isTeacher) {
            req->send(200, "application/json", "{\"success\":false}");
            return;
        }
        if (!isTeacher && !isUser) {
            req->send(200, "application/json", "{\"success\":false}");
            return;
        }
    }
    if (req->hasArg("temp"))  {
        ctx.setTargetTemp = req->arg("temp").toFloat();
        storage_saveFloat("targetTemp", ctx.setTargetTemp);  // ← save
    }
    if (req->hasArg("range")) {
        ctx.setTempRange = req->arg("range").toFloat();
        storage_saveFloat("tempRange", ctx.setTempRange);    // ← save
    }
    if (req->hasArg("wind")) {
        ctx.windThreshold = req->arg("wind").toFloat();
        storage_saveFloat("windThresh", ctx.windThreshold);  // ← save
    }
    req->send(200, "application/json", "{\"success\":true}");
});

    // ── POST /api/override ───────────────────────────────────────
    server.on("/api/override", HTTP_POST, [](AsyncWebServerRequest* req) {
        SystemContext& ctx = *sysCtxPtr;
        if (!isTeacherLoggedIn(req)) {
            req->send(200, "application/json", "{\"success\":false}");
            return;
        }
        
        bool enable = req->arg("enable") == "1";
        // teacher can always disable override
    // teacher can only enable if not already overriding
        ctx.teacherOverride = enable;
        req->send(200, "application/json", "{\"success\":true}");
    });

    // ── POST /api/password ───────────────────────────────────────
    server.on("/api/password", HTTP_POST, [](AsyncWebServerRequest* req) {
        SystemContext& ctx = *sysCtxPtr;
        if (!isTeacherLoggedIn(req)) {
            req->send(200, "application/json", "{\"success\":false}");
            return;
        }
        String type = req->arg("type");
        String pass = req->arg("pass");
        if (pass.length() < 4) {
            req->send(200, "application/json", "{\"success\":false}");
            return;
        }
        if (type == "user") {
            pass.toCharArray(ctx.userPassword, 32);
            storage_saveUserPassword(ctx.userPassword);
        } else if (type == "teacher") {
            pass.toCharArray(ctx.teacherPassword, 32);
            storage_saveTeacherPassword(ctx.teacherPassword);
        }
        req->send(200, "application/json", "{\"success\":true}");
    });

    // ── POST /api/resetwifi ──────────────────────────────────────
    server.on("/api/resetwifi", HTTP_POST, [](AsyncWebServerRequest* req) {
        if (!isTeacherLoggedIn(req)) {
            req->send(200, "application/json", "{\"success\":false}");
            return;
        }
        req->send(200, "application/json", "{\"success\":true}");
        delay(500);
        storage_clearWiFi();
        ESP.restart();
    });

    server.begin();
    Serial.print("[WEBSERVER] Dashboard at http://");
    Serial.println(WiFi.localIP());
}

// ================================================================
// UPDATE — nothing needed, AsyncWebServer handles in background
// ================================================================
void webserver_update(SystemContext& ctx) {
    // AsyncWebServer is interrupt driven — no polling needed
}