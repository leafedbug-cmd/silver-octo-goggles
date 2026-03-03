#include "WiFiController.h"

WiFiController::WiFiController(ESP32NRF24Jammer& jammer)
    : _jammer(jammer), _server(80), _ssid(""), _password(""), _running(false) {
    strcpy(_ipAddr, "192.168.4.1");
}

bool WiFiController::begin(const char* ssid, const char* password) {
    _ssid = ssid;
    _password = password;
    
    // Start WiFi AP mode
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
    
    return true;
}

void WiFiController::setupRoutes() {
    _server.on("/", HTTP_GET, [this]() { handleRoot(); });
    _server.on("/api/status", HTTP_GET, [this]() { handleJammingStatus(); });
    _server.on("/api/jamming", HTTP_POST, [this]() { handleJammingToggle(); });
    _server.on("/api/channel", HTTP_POST, [this]() { handleSetChannel(); });
    _server.on("/api/frequency_step", HTTP_POST, [this]() { handleSetFrequencyStep(); });
    _server.onNotFound([this]() { handleNotFound(); });
}

void WiFiController::handleClient() {
    _server.handleClient();
}

void WiFiController::handleRoot() {
    _server.send(200, "text/html", generateHTML());
}

void WiFiController::handleJammingStatus() {
    String json = "{\"jamming\":" + String(_jammer.isJamming() ? "true" : "false") + "}";
    _server.send(200, "application/json", json);
}

void WiFiController::handleJammingToggle() {
    if (_jammer.isJamming()) {
        _jammer.jammingOff();
        _server.send(200, "application/json", "{\"status\":\"jamming off\"}");
    } else {
        _jammer.jammingOn();
        _server.send(200, "application/json", "{\"status\":\"jamming on\"}");
    }
}

void WiFiController::handleSetChannel() {
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

void WiFiController::handleNotFound() {
    _server.send(404, "text/plain", "404 Not Found");
}

String WiFiController::generateHTML() {
    return String(
        "<!DOCTYPE html>"
        "<html><head>"
        "<meta charset=\"UTF-8\">"
        "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
        "<title>nRF24 Jammer Control</title>"
        "<style>"
        "* { margin: 0; padding: 0; box-sizing: border-box; }"
        "body {"
        "  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;"
        "  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);"
        "  min-height: 100vh;"
        "  display: flex;"
        "  justify-content: center;"
        "  align-items: center;"
        "  padding: 20px;"
        "}"
        ".container {"
        "  background: white;"
        "  border-radius: 12px;"
        "  box-shadow: 0 10px 40px rgba(0,0,0,0.2);"
        "  padding: 40px;"
        "  max-width: 500px;"
        "  width: 100%;"
        "}"
        "h1 {"
        "  color: #333;"
        "  margin-bottom: 30px;"
        "  text-align: center;"
        "  font-size: 28px;"
        "}"
        ".status-box {"
        "  background: #f5f5f5;"
        "  border-left: 4px solid #667eea;"
        "  padding: 15px;"
        "  margin-bottom: 30px;"
        "  border-radius: 4px;"
        "}"
        ".status-label {"
        "  font-size: 12px;"
        "  color: #666;"
        "  text-transform: uppercase;"
        "  letter-spacing: 1px;"
        "  margin-bottom: 8px;"
        "}"
        ".status-value {"
        "  font-size: 20px;"
        "  font-weight: bold;"
        "  color: #333;"
        "}"
        ".status-value.active { color: #ff4444; }"
        ".status-value.inactive { color: #44aa44; }"
        ".control-group { margin-bottom: 25px; }"
        "label {"
        "  display: block;"
        "  font-size: 14px;"
        "  color: #333;"
        "  margin-bottom: 8px;"
        "  font-weight: 500;"
        "}"
        "input[type=\"number\"] {"
        "  width: 100%;"
        "  padding: 12px;"
        "  border: 1px solid #ddd;"
        "  border-radius: 4px;"
        "  font-size: 16px;"
        "  transition: border-color 0.3s;"
        "}"
        "input[type=\"number\"]:focus {"
        "  outline: none;"
        "  border-color: #667eea;"
        "}"
        "button {"
        "  width: 100%;"
        "  padding: 14px;"
        "  border: none;"
        "  border-radius: 4px;"
        "  font-size: 16px;"
        "  font-weight: bold;"
        "  cursor: pointer;"
        "  transition: all 0.3s;"
        "  text-transform: uppercase;"
        "  letter-spacing: 0.5px;"
        "}"
        ".btn-primary {"
        "  background: #667eea;"
        "  color: white;"
        "  margin-bottom: 10px;"
        "}"
        ".btn-primary:hover {"
        "  background: #5568d3;"
        "  transform: translateY(-2px);"
        "  box-shadow: 0 5px 15px rgba(102, 126, 234, 0.4);"
        "}"
        ".btn-danger {"
        "  background: #ff4444;"
        "  color: white;"
        "}"
        ".btn-danger:hover {"
        "  background: #dd3333;"
        "  transform: translateY(-2px);"
        "  box-shadow: 0 5px 15px rgba(255, 68, 68, 0.4);"
        "}"
        ".btn-secondary {"
        "  background: #9ca3af;"
        "  color: white;"
        "}"
        ".btn-secondary:hover {"
        "  background: #6b7280;"
        "  transform: translateY(-2px);"
        "}"
        ".button-group {"
        "  display: grid;"
        "  grid-template-columns: 1fr 1fr;"
        "  gap: 10px;"
        "}"
        ".info {"
        "  background: #f0f4ff;"
        "  border-left: 4px solid #667eea;"
        "  padding: 12px;"
        "  margin-top: 20px;"
        "  border-radius: 4px;"
        "  font-size: 12px;"
        "  color: #555;"
        "}"
        "</style>"
        "</head>"
        "<body>"
        "<div class=\"container\">"
        "<h1>Jammer Control</h1>"
        "<div class=\"status-box\">"
        "<div class=\"status-label\">Jamming Status</div>"
        "<div class=\"status-value\" id=\"jamming-status\">Loading...</div>"
        "</div>"
        "<div class=\"button-group\">"
        "<button class=\"btn-primary\" onclick=\"toggleJamming()\">Toggle Jamming</button>"
        "<button class=\"btn-secondary\" onclick=\"refreshStatus()\">Refresh</button>"
        "</div>"
        "<div class=\"control-group\">"
        "<label for=\"channel\">Channel (0-124)</label>"
        "<input type=\"number\" id=\"channel\" min=\"0\" max=\"124\" value=\"0\" placeholder=\"Enter channel\">"
        "<button class=\"btn-primary\" onclick=\"setChannel()\" style=\"margin-top: 8px;\">Set Channel</button>"
        "</div>"
        "<div class=\"control-group\">"
        "<label for=\"freq-step\">Frequency Step (Hz)</label>"
        "<input type=\"number\" id=\"freq-step\" min=\"1\" max=\"5000000\" value=\"500000\" placeholder=\"Enter frequency step\">"
        "<button class=\"btn-primary\" onclick=\"setFrequencyStep()\" style=\"margin-top: 8px;\">Set Step</button>"
        "</div>"
        "<div class=\"info\">"
        "<strong>Info:</strong> Channel range: 0-124 (Default step: 500kHz)<br>"
        "Jamming will be highlighted in red when active."
        "</div>"
        "</div>"
        "<script>"
        "async function refreshStatus() {"
        "  try {"
        "    const res = await fetch('/api/status');"
        "    const data = await res.json();"
        "    const elem = document.getElementById('jamming-status');"
        "    elem.textContent = data.jamming ? 'ACTIVE' : 'INACTIVE';"
        "    elem.className = 'status-value ' + (data.jamming ? 'active' : 'inactive');"
        "  } catch (e) { console.log('Error:', e); }"
        "}"
        "async function toggleJamming() {"
        "  try {"
        "    const res = await fetch('/api/jamming', { method: 'POST' });"
        "    const data = await res.json();"
        "    refreshStatus();"
        "  } catch (e) { console.log('Error:', e); }"
        "}"
        "async function setChannel() {"
        "  const channel = document.getElementById('channel').value;"
        "  if (channel == '' || channel < 0 || channel > 124) {"
        "    alert('Invalid channel (0-124)');"
        "    return;"
        "  }"
        "  try {"
        "    const res = await fetch('/api/channel?channel=' + channel, { method: 'POST' });"
        "    const data = await res.json();"
        "    if (data.error) {"
        "      alert('Error: ' + data.error);"
        "    } else {"
        "      alert('Channel set to ' + data.channel);"
        "    }"
        "  } catch (e) { console.log('Error:', e); }"
        "}"
        "async function setFrequencyStep() {"
        "  const step = document.getElementById('freq-step').value;"
        "  if (step == '' || step < 1 || step > 5000000) {"
        "    alert('Invalid step (1-5000000)');"
        "    return;"
        "  }"
        "  try {"
        "    const res = await fetch('/api/frequency_step?step=' + step, { method: 'POST' });"
        "    const data = await res.json();"
        "    if (data.error) {"
        "      alert('Error: ' + data.error);"
        "    } else {"
        "      alert('Frequency step set to ' + data.step);"
        "    }"
        "  } catch (e) { console.log('Error:', e); }"
        "}"
        "refreshStatus();"
        "setInterval(refreshStatus, 2000);"
        "</script>"
        "</body></html>"
    );
}
