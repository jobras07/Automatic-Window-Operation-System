#include "portal.h"
#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>

static AsyncWebServer portalServer(80);
static DNSServer      dnsServer;
static bool           wifiConnected   = false;
static bool           portalActive    = false;

#define PORTAL_SSID     "AWO_Setup"
#define PORTAL_TIMEOUT  180000  // 3 minutes to connect before giving up

// ================================================================
// PORTAL HTML
// ================================================================
static const char PORTAL_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>AWO WiFi Setup</title>
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body { font-family: Arial, sans-serif; background: #1a1a2e; color: #eee; 
               display: flex; justify-content: center; align-items: center; 
               min-height: 100vh; padding: 20px; }
        .card { background: #16213e; border-radius: 12px; padding: 30px; 
                width: 100%; max-width: 400px; box-shadow: 0 4px 20px rgba(0,0,0,0.3); }
        h1 { color: #0f3460; margin-bottom: 8px; font-size: 1.5em; }
        h1 span { color: #e94560; }
        p { color: #aaa; margin-bottom: 24px; font-size: 0.9em; }
        label { display: block; margin-bottom: 6px; font-size: 0.85em; color: #aaa; }
        select, input { width: 100%; padding: 12px; border-radius: 8px; border: 1px solid #0f3460;
                        background: #1a1a2e; color: #eee; font-size: 1em; margin-bottom: 16px; }
        button { width: 100%; padding: 14px; background: #e94560; color: white;
                 border: none; border-radius: 8px; font-size: 1em; cursor: pointer; }
        button:hover { background: #c73652; }
        .status { margin-top: 16px; padding: 12px; border-radius: 8px; 
                  text-align: center; display: none; }
        .success { background: #1a472a; color: #69db7c; display: block; }
        .error   { background: #4a1942; color: #f783ac; display: block; }
    </style>
</head>
<body>
    <div class="card">
        <h1>AWO <span>Setup</span></h1>
        <p>Connect your Automatic Window Opener to your WiFi network.</p>
        <label>Select Network</label>
        <select id="ssid">
            <option value="">Scanning...</option>
        </select>
        <label>Password</label>
        <input type="password" id="pass" placeholder="WiFi Password">
        <button onclick="connect()">Connect</button>
        <div id="status" class="status"></div>
    </div>
    <script>
        // scan for networks on load
        fetch('/scan')
            .then(r => r.json())
            .then(networks => {
                const select = document.getElementById('ssid');
                select.innerHTML = networks.map(n => 
                    `<option value="${n.ssid}">${n.ssid} (${n.rssi}dBm)</option>`
                ).join('');
            });

        function connect() {
            const ssid = document.getElementById('ssid').value;
            const pass = document.getElementById('pass').value;
            const status = document.getElementById('status');

            if (!ssid) { 
                status.className = 'status error';
                status.textContent = 'Please select a network';
                return; 
            }

            status.className = 'status';
            status.textContent = 'Connecting...';
            status.style.display = 'block';

            fetch('/connect', {
                method: 'POST',
                headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                body: `ssid=${encodeURIComponent(ssid)}&pass=${encodeURIComponent(pass)}`
            })
            .then(r => r.json())
            .then(data => {
                if (data.success) {
                    status.className = 'status success';
                    status.textContent = 'Connected! IP: ' + data.ip;
                } else {
                    status.className = 'status error';
                    status.textContent = 'Failed. Check password and try again.';
                }
            });
        }
    </script>
</body>
</html>
)rawliteral";

// ================================================================
// INIT
// ================================================================
void portal_start() {
    Serial.println("[PORTAL] Starting captive portal...");

    WiFi.mode(WIFI_AP);
    WiFi.softAP(PORTAL_SSID);
    delay(100);

    // DNS server — redirects all domains to ESP32
    dnsServer.start(53, "*", WiFi.softAPIP());

    // serve portal page
    portalServer.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send(200, "text/html", PORTAL_HTML);
    });

    // redirect all unknown URLs to portal (captive portal behavior)
    portalServer.onNotFound([](AsyncWebServerRequest* req) {
        req->redirect("/");
    });

    // scan networks
    portalServer.on("/scan", HTTP_GET, [](AsyncWebServerRequest* req) {
        int n = WiFi.scanNetworks();
        String json = "[";
        for (int i = 0; i < n; i++) {
            if (i > 0) json += ",";
            json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",\"rssi\":" + WiFi.RSSI(i) + "}";
        }
        json += "]";
        req->send(200, "application/json", json);
    });

    // handle connect
    portalServer.on("/connect", HTTP_POST, [](AsyncWebServerRequest* req) {
        String ssid = req->arg("ssid");
        String pass = req->arg("pass");

        // try connecting
        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid.c_str(), pass.c_str());

        int timeout = 0;
        while (WiFi.status() != WL_CONNECTED && timeout < 20) {
            delay(500);
            timeout++;
        }

        if (WiFi.status() == WL_CONNECTED) {
            storage_saveWiFi(ssid.c_str(), pass.c_str());
            wifiConnected = true;
            String ip = WiFi.localIP().toString();
            req->send(200, "application/json", 
                "{\"success\":true,\"ip\":\"" + ip + "\"}");
        } else {
            WiFi.mode(WIFI_AP);
            WiFi.softAP(PORTAL_SSID);
            req->send(200, "application/json", "{\"success\":false}");
        }
    });

    portalServer.begin();
    portalActive = true;

    Serial.print("[PORTAL] AP IP: ");
    Serial.println(WiFi.softAPIP());
}

// ================================================================
// UPDATE — call every loop() while portal is active
// ================================================================
bool portal_update(SystemContext& ctx) {
    if (!portalActive) return false;
    dnsServer.processNextRequest();
    return wifiConnected;
}

// ================================================================
// STOP
// ================================================================
void portal_stop() {
    portalServer.end();
    dnsServer.stop();
    portalActive = false;
    Serial.println("[PORTAL] Stopped");
}