#include "WiFiController.h"

WiFiController::WiFiController(ESP32NRF24Jammer& jammer)
    : _jammer(jammer), _server(80), _ssid(""), _password(""), _running(false), _initialized(false) {
    strcpy(_ipAddr, "192.168.4.1");
}

bool WiFiController::begin(const char* ssid, const char* password) {
    _ssid = ssid;
    _password = password;

    WiFi.mode(WIFI_AP);
    
    Serial.print("Starting WiFi AP: ");
    Serial.println(_ssid);

    if (!WiFi.softAP(_ssid, _password)) {
        Serial.println("Failed to start AP!");
        return false;
    }
    
    // Get IP address
    IPAddress ip = WiFi.softAPIP();
    sprintf(_ipAddr, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    
    Serial.print("AP IP address: ");
    Serial.println(_ipAddr);
    
    setupRoutes();
    _server.begin();
    _running = true;
    _initialized = true;  // WiFi is initialized, radio will be done separately
    
    return true;
}

void WiFiController::setupRoutes() {
    _server.on("/", HTTP_GET, [this]() { handleRoot(); });
    _server.on("/api/status", HTTP_GET, [this]() { handleJammingStatus(); });
    _server.on("/api/jamming", HTTP_POST, [this]() { handleJammingToggle(); });
    _server.on("/api/channel", HTTP_POST, [this]() { handleSetChannel(); });
    _server.on("/api/frequency_step", HTTP_POST, [this]() { handleSetFrequencyStep(); });
    _server.on("/api/init", HTTP_POST, [this]() { handleInit(); });
    _server.on("/api/scan", HTTP_GET, [this]() { handleScan(); });
    _server.onNotFound([this]() { handleNotFound(); });
}

void WiFiController::handleClient() {
    _server.handleClient();
}

void WiFiController::handleRoot() {
    _server.send(200, "text/html", generateHTML());
}

void WiFiController::handleJammingStatus() {
    String json = "{\"jamming\":" + String(_jammer.isJamming() ? "true" : "false") 
        + ",\"scanning\":" + String(_jammer.isScanReady() ? "true" : "false")
        + "}";
    _server.send(200, "application/json", json);
}

void WiFiController::handleJammingToggle() {
    if (!_initialized) {
        _server.send(400, "application/json", "{\"error\":\"not initialized\"}");
        return;
    }
    if (_jammer.isJamming()) {
        _jammer.jammingOff();
        _server.send(200, "application/json", "{\"status\":\"jamming off\"}");
    } else {
        _jammer.jammingOn();
        _server.send(200, "application/json", "{\"status\":\"jamming on\"}");
    }
}

void WiFiController::handleSetChannel() {
    if (!_initialized) {
        _server.send(400, "application/json", "{\"error\":\"not initialized\"}");
        return;
    }
    if (_server.hasArg("channel")) {
        int channel = _server.arg("channel").toInt();
        if (channel >= 0 && channel <= 124) {
            _jammer.setChannel(channel);
            _server.send(200, "application/json", "{\"channel\":" + String(channel) + "}");
        } else {
            _server.send(400, "application/json", "{\"error\":\"invalid channel\"}");
        }
    } else {
        _server.send(400, "application/json", "{\"error\":\"missing channel parameter\"}");
    }
}

void WiFiController::handleSetFrequencyStep() {
    if (!_initialized) {
        _server.send(400, "application/json", "{\"error\":\"not initialized\"}");
        return;
    }
    if (_server.hasArg("step")) {
        uint32_t step = _server.arg("step").toInt();
        if (step > 0 && step <= 5000000) {
            _jammer.setFrequencyStep(step);
            _server.send(200, "application/json", "{\"step\":" + String(step) + "}");
        } else {
            _server.send(400, "application/json", "{\"error\":\"invalid step\"}");
        }
    } else {
        _server.send(400, "application/json", "{\"error\":\"missing step parameter\"}");
    }
}

void WiFiController::handleInit() {
    // Jamming is now controlled via physical button only
    _server.send(200, "application/json", "{\"message\":\"Use physical BOOT button to toggle jamming\"}");
}

void WiFiController::handleScan() {
    // Return cached scan results instantly (no blocking!)
    if (!_jammer.isScanReady()) {
        _server.send(200, "application/json", "{\"channels\":[],\"freqStart\":2400,\"freqEnd\":2525,\"ready\":false}");
        return;
    }
    
    uint8_t scanResults[126];
    _jammer.getScanResults(scanResults);
    
    String json = "{\"channels\":[";
    for (int i = 0; i < 126; i++) {
        json += String(scanResults[i]);
        if (i < 125) json += ",";
    }
    json += "],\"freqStart\":2400,\"freqEnd\":2525,\"ready\":true}";
    
    _server.send(200, "application/json", json);
}

void WiFiController::handleNotFound() {
    _server.send(404, "text/plain", "404 Not Found");
}

String WiFiController::generateHTML() {
    return String(
        "<!DOCTYPE html>"
        "<html><head>"
        "<meta charset=\"UTF-8\">"
        "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
        "<title>2.4GHz Scanner</title>"
        "<style>"
        "* { margin: 0; padding: 0; box-sizing: border-box; }"
        "body {"
        "  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;"
        "  background: #1a1a2e;"
        "  min-height: 100vh;"
        "  padding: 20px;"
        "  color: #eee;"
        "}"
        ".container { max-width: 800px; margin: 0 auto; }"
        "h1 {"
        "  color: #00ff88;"
        "  margin-bottom: 20px;"
        "  text-align: center;"
        "  font-size: 24px;"
        "  text-shadow: 0 0 10px #00ff8844;"
        "}"
        ".panel {"
        "  background: #16213e;"
        "  border-radius: 8px;"
        "  padding: 20px;"
        "  margin-bottom: 20px;"
        "  border: 1px solid #0f3460;"
        "}"
        ".panel h2 {"
        "  color: #00ff88;"
        "  font-size: 14px;"
        "  margin-bottom: 15px;"
        "  text-transform: uppercase;"
        "  letter-spacing: 2px;"
        "}"
        ".spectrum-container {"
        "  background: #0a0a1a;"
        "  border-radius: 4px;"
        "  padding: 10px;"
        "}"
        "#spectrum {"
        "  width: 100%;"
        "  height: 200px;"
        "  background: #000;"
        "  border-radius: 4px;"
        "}"
        ".freq-labels {"
        "  display: flex;"
        "  justify-content: space-between;"
        "  font-size: 10px;"
        "  color: #666;"
        "  margin-top: 5px;"
        "}"
        ".legend {"
        "  display: flex;"
        "  gap: 15px;"
        "  font-size: 10px;"
        "  margin-top: 10px;"
        "  justify-content: center;"
        "}"
        ".legend span { display: flex; align-items: center; gap: 5px; }"
        ".legend .dot { width: 8px; height: 8px; border-radius: 50%; }"
        ".wifi-dot { background: #00ff88; }"
        ".bt-dot { background: #00aaff; }"
        ".other-dot { background: #ffaa00; }"
        ".device-list { max-height: 300px; overflow-y: auto; }"
        ".device-item {"
        "  display: flex;"
        "  justify-content: space-between;"
        "  padding: 8px 10px;"
        "  background: #0a0a1a;"
        "  border-radius: 4px;"
        "  margin-bottom: 4px;"
        "  font-size: 12px;"
        "}"
        ".device-item .type { color: #00ff88; font-weight: bold; }"
        ".device-item .freq { color: #888; }"
        ".device-item .strength { color: #ffaa00; }"
        ".no-devices { color: #666; text-align: center; padding: 20px; font-size: 12px; }"
        "</style>"
        "</head>"
        "<body>"
        "<div class=\"container\">"
        "<h1>2.4GHz SPECTRUM SCANNER</h1>"
        "<div class=\"panel\">"
        "<h2>Live Spectrum</h2>"
        "<div class=\"spectrum-container\">"
        "<canvas id=\"spectrum\"></canvas>"
        "<div class=\"freq-labels\">"
        "<span>2400 MHz</span><span>WiFi Ch1</span><span>Ch6</span><span>Ch11</span><span>2525 MHz</span>"
        "</div>"
        "<div class=\"legend\">"
        "<span><div class=\"dot wifi-dot\"></div>WiFi</span>"
        "<span><div class=\"dot bt-dot\"></div>Bluetooth</span>"
        "<span><div class=\"dot other-dot\"></div>Other</span>"
        "</div>"
        "</div>"
        "</div>"
        "<div class=\"panel\">"
        "<h2>Detected Devices</h2>"
        "<div id=\"devices\" class=\"device-list\">"
        "<div class=\"no-devices\">Scanning...</div>"
        "</div>"
        "</div>"
        "</div>"
        "<script>"
        "let canvas, ctx;"
        "window.onload = function() {"
        "  canvas = document.getElementById('spectrum');"
        "  ctx = canvas.getContext('2d');"
        "  canvas.width = canvas.offsetWidth;"
        "  canvas.height = canvas.offsetHeight;"
        "  drawGrid();"
        "  setInterval(scanSpectrum, 500);"
        "  scanSpectrum();"
        "};"
        "function drawGrid() {"
        "  ctx.fillStyle = '#000';"
        "  ctx.fillRect(0, 0, canvas.width, canvas.height);"
        "  ctx.strokeStyle = '#1a1a3a';"
        "  ctx.lineWidth = 1;"
        "  for(let i = 0; i < canvas.width; i += 20) {"
        "    ctx.beginPath(); ctx.moveTo(i, 0); ctx.lineTo(i, canvas.height); ctx.stroke();"
        "  }"
        "  for(let i = 0; i < canvas.height; i += 20) {"
        "    ctx.beginPath(); ctx.moveTo(0, i); ctx.lineTo(canvas.width, i); ctx.stroke();"
        "  }"
        "}"
        "function drawSpectrum(data) {"
        "  drawGrid();"
        "  const w = canvas.width / 126;"
        "  for(let i = 0; i < 126; i++) {"
        "    const h = (data[i] / 100) * canvas.height;"
        "    let color;"
        "    const freq = 2400 + i;"
        "    if(freq >= 2402 && freq <= 2480) color = '#00aaff';"
        "    else if(freq >= 2412 && freq <= 2484) color = '#00ff88';"
        "    else color = '#ffaa00';"
        "    const grad = ctx.createLinearGradient(0, canvas.height - h, 0, canvas.height);"
        "    grad.addColorStop(0, color);"
        "    grad.addColorStop(1, color + '44');"
        "    ctx.fillStyle = grad;"
        "    ctx.fillRect(i * w, canvas.height - h, w - 1, h);"
        "  }"
        "}"
        "async function scanSpectrum() {"
        "  try {"
        "    const res = await fetch('/api/scan');"
        "    const data = await res.json();"
        "    if(data.channels && data.channels.length > 0) {"
        "      drawSpectrum(data.channels);"
        "      updateDeviceList(data.channels);"
        "    }"
        "  } catch(e) {}"
        "}"
        "function updateDeviceList(channels) {"
        "  const devices = [];"
        "  let i = 0;"
        "  while (i < 126) {"
        "    if (channels[i] > 20) {"
        "      let start = i;"
        "      let maxStr = channels[i];"
        "      while (i < 126 && channels[i] > 10) {"
        "        if (channels[i] > maxStr) maxStr = channels[i];"
        "        i++;"
        "      }"
        "      let end = i - 1;"
        "      let freqStart = 2400 + start;"
        "      let freqEnd = 2400 + end;"
        "      let type = 'RF Signal';"
        "      let width = end - start;"
        "      if (width >= 15 && width <= 25) type = 'WiFi';"
        "      else if (width <= 3) type = 'Bluetooth';"
        "      else if (width > 25) type = 'Wide Band';"
        "      devices.push({type, freqStart, freqEnd, strength: maxStr});"
        "    }"
        "    i++;"
        "  }"
        "  const container = document.getElementById('devices');"
        "  if (devices.length === 0) {"
        "    container.innerHTML = '<div class=\"no-devices\">No devices detected</div>';"
        "  } else {"
        "    container.innerHTML = devices.map(d =>"
        "      '<div class=\"device-item\">'+"
        "      '<span class=\"type\">' + d.type + '</span>'+"
        "      '<span class=\"freq\">' + d.freqStart + '-' + d.freqEnd + ' MHz</span>'+"
        "      '<span class=\"strength\">' + d.strength + '%</span>'+"
        "      '</div>'"
        "    ).join('');"
        "  }"
        "}"
        "</script>"
        "</body></html>"
    );
}
