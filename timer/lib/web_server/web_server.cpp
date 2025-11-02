#include "web_server.h"
#include <WiFi.h>
#include <ArduinoJson.h>
#include <module_tracker.h>

static WiFiServer* server = nullptr;
static GameStateManager* gameState = nullptr;
static WiFiClient currentClient;

// WiFi credentials (change these or use AP mode)
const char* ssid = "KTANE_TIMER";
const char* password = "ktane12345";

// Helper function to send HTTP response
void sendResponse(WiFiClient& client, int code, const String& contentType, const String& body) {
    client.print("HTTP/1.1 ");
    client.print(code);
    client.println(" OK");
    client.print("Content-Type: ");
    client.println(contentType);
    client.print("Content-Length: ");
    client.println(body.length());
    client.println("Connection: close");
    client.println();
    client.print(body);
}

// Helper function to format game state
String formatGameState(GameState state) {
    switch (state) {
        case GameState::IDLE: return "IDLE";
        case GameState::RUNNING: return "RUNNING";
        case GameState::PAUSED: return "PAUSED";
        case GameState::EXPLODED: return "EXPLODED";
        case GameState::DEFUSED: return "DEFUSED";
        case GameState::VICTORY: return "VICTORY";
        default: return "UNKNOWN";
    }
}

// Parse HTTP request
bool parseRequest(WiFiClient& client, String& method, String& path) {
    String request = client.readStringUntil('\r');
    client.read(); // skip \n
    
    int firstSpace = request.indexOf(' ');
    int secondSpace = request.indexOf(' ', firstSpace + 1);
    
    if (firstSpace == -1 || secondSpace == -1) {
        return false;
    }
    
    method = request.substring(0, firstSpace);
    path = request.substring(firstSpace + 1, secondSpace);
    
    // Read rest of headers
    while (client.available()) {
        String line = client.readStringUntil('\r');
        client.read(); // skip \n
        if (line.length() == 0) break;
    }
    
    return true;
}

// Handle status API
void handleStatus(WiFiClient& client) {
    if (!gameState) {
        sendResponse(client, 500, "application/json", "{\"success\":false,\"error\":\"Game state not initialized\"}");
        return;
    }

    DynamicJsonDocument doc(1024);
    doc["success"] = true;
    doc["state"] = formatGameState(gameState->getState());
    doc["timeRemaining"] = gameState->getRemainingTime();
    doc["strikes"] = gameState->getStrikes();
    doc["maxStrikes"] = gameState->getMaxStrikes();
    doc["totalModules"] = gameState->getTotalModules();
    doc["solvedModules"] = gameState->getSolvedModules();
    doc["remainingModules"] = gameState->getTotalModules() - gameState->getSolvedModules();
    
    doc["serialNumber"] = gameState->getSerialNumber();
    doc["batteries"] = gameState->getBatteryCount();
    
    const Edgework& edge = gameState->getEdgework();
    doc["indicators"] = edge.indicators.size();
    doc["ports"] = edge.ports.size();

    String response;
    serializeJson(doc, response);
    sendResponse(client, 200, "application/json", response);
}

// Handle command API
void handleCommand(WiFiClient& client, String body) {
    if (!gameState) {
        sendResponse(client, 500, "application/json", "{\"success\":false,\"error\":\"Game state not initialized\"}");
        return;
    }

    DynamicJsonDocument doc(200);
    DeserializationError error = deserializeJson(doc, body);
    
    if (error || !doc.containsKey("command")) {
        sendResponse(client, 400, "application/json", "{\"success\":false,\"error\":\"Invalid request\"}");
        return;
    }

    String cmd = doc["command"].as<String>();
    bool success = true;
    String errorMsg = "";

    if (cmd == "start") {
        if (gameState->getState() == GameState::IDLE) {
            gameState->startGame();
        } else if (gameState->getState() == GameState::PAUSED) {
            gameState->resumeTimer();
        } else {
            success = false;
            errorMsg = "Cannot start from current state";
        }
    } else if (cmd == "pause") {
        if (gameState->getState() == GameState::RUNNING) {
            gameState->pauseTimer();
        } else {
            success = false;
            errorMsg = "Game is not running";
        }
    } else if (cmd == "reset") {
        gameState->reset();
    } else if (cmd == "addStrike") {
        gameState->addStrike();
    } else if (cmd == "clearStrikes") {
        gameState->clearStrikes();
    } else if (cmd == "solveModule") {
        // Find first unsolved module and solve it
        for (uint16_t i = 0x10; i <= 0x6F; i++) {
            if (gameState->getModule(i) && !gameState->isModuleSolved(i)) {
                gameState->setModuleSolved(i);
                break;
            }
        }
    } else if (cmd.startsWith("setTime:")) {
        String timeStr = cmd.substring(8);
        int colonIndex = timeStr.indexOf(':');
        if (colonIndex != -1) {
            int mins = timeStr.substring(0, colonIndex).toInt();
            int secs = timeStr.substring(colonIndex + 1).toInt();
            unsigned long customCountdownMillis = (mins * 60UL + secs) * 1000UL;
            gameState->setTimeLimit(customCountdownMillis);
            gameState->resetTimer();
        } else {
            success = false;
            errorMsg = "Invalid time format";
        }
    } else {
        success = false;
        errorMsg = "Unknown command";
    }

    DynamicJsonDocument response(200);
    response["success"] = success;
    if (!success) response["error"] = errorMsg;
    String responseStr;
    serializeJson(response, responseStr);
    sendResponse(client, success ? 200 : 400, "application/json", responseStr);
}

// Handle get config
void handleGetConfig(WiFiClient& client) {
    if (!gameState) {
        sendResponse(client, 500, "application/json", "{\"success\":false,\"error\":\"Game state not initialized\"}");
        return;
    }

    GameConfig config = gameState->getConfig();
    DynamicJsonDocument doc(512);
    doc["success"] = true;
    doc["config"]["maxStrikes"] = config.maxStrikes;
    doc["config"]["enableStrikeAcceleration"] = config.enableStrikeAcceleration;
    doc["config"]["strikeAccelerationFactor"] = config.strikeAccelerationFactor;
    doc["config"]["enableEmergencyAlarm"] = config.enableEmergencyAlarm;
    doc["config"]["emergencyAlarmThreshold"] = config.emergencyAlarmThreshold;
    doc["config"]["enableNeedyModules"] = config.enableNeedyModules;
    doc["config"]["enableEdgework"] = config.enableEdgework;

    String response;
    serializeJson(doc, response);
    sendResponse(client, 200, "application/json", response);
}

// Handle set config
void handleSetConfig(WiFiClient& client, String body) {
    if (!gameState) {
        sendResponse(client, 500, "application/json", "{\"success\":false,\"error\":\"Game state not initialized\"}");
        return;
    }

    DynamicJsonDocument doc(512);
    DeserializationError error = deserializeJson(doc, body);
    
    if (error) {
        sendResponse(client, 400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
        return;
    }

    GameConfig config;
    config.maxStrikes = doc["maxStrikes"] | 3;
    config.enableStrikeAcceleration = doc["enableStrikeAcceleration"] | true;
    config.strikeAccelerationFactor = doc["strikeAccelerationFactor"] | 0.25f;
    config.enableEmergencyAlarm = doc["enableEmergencyAlarm"] | true;
    config.emergencyAlarmThreshold = doc["emergencyAlarmThreshold"] | 60000;
    config.enableNeedyModules = doc["enableNeedyModules"] | true;
    config.enableEdgework = doc["enableEdgework"] | true;
    config.timeLimitMs = 300000; // Keep existing time limit

    gameState->setConfig(config);

    DynamicJsonDocument response(200);
    response["success"] = true;
    String responseStr;
    serializeJson(response, responseStr);
    sendResponse(client, 200, "application/json", responseStr);
}

// HTML page with Tailwind CSS (keeping the same HTML)
const char* html_page = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>KTANE Timer Control</title>
    <script src="https://cdn.tailwindcss.com"></script>
    <script>
        tailwind.config = {
            theme: {
                extend: {
                    colors: {
                        'ktane-red': '#8B0000',
                        'ktane-green': '#00FF00',
                        'ktane-yellow': '#FFD700'
                    }
                }
            }
        }
    </script>
</head>
<body class="bg-gray-900 text-white min-h-screen">
    <div class="container mx-auto px-4 py-8 max-w-4xl">
        <!-- Header -->
        <div class="text-center mb-8">
            <h1 class="text-4xl font-bold mb-2 bg-gradient-to-r from-red-500 to-orange-500 bg-clip-text text-transparent">
                ⏱️ KTANE Timer Control
            </h1>
            <p class="text-gray-400">Master Bomb Control Interface</p>
        </div>

        <!-- Status Cards -->
        <div class="grid grid-cols-1 md:grid-cols-3 gap-4 mb-6" id="statusCards">
            <!-- State Card -->
            <div class="bg-gray-800 rounded-lg p-4 border-2" id="stateCard">
                <div class="text-sm text-gray-400 mb-1">Game State</div>
                <div class="text-2xl font-bold" id="gameState">IDLE</div>
            </div>
            
            <!-- Time Card -->
            <div class="bg-gray-800 rounded-lg p-4 border-2">
                <div class="text-sm text-gray-400 mb-1">Time Remaining</div>
                <div class="text-2xl font-bold" id="timeRemaining">5:00</div>
            </div>
            
            <!-- Strikes Card -->
            <div class="bg-gray-800 rounded-lg p-4 border-2" id="strikeCard">
                <div class="text-sm text-gray-400 mb-1">Strikes</div>
                <div class="text-2xl font-bold" id="strikes">0/3</div>
            </div>
        </div>

        <!-- Game Control Panel -->
        <div class="bg-gray-800 rounded-lg p-6 mb-6 border border-gray-700">
            <h2 class="text-xl font-bold mb-4 pb-2 border-b border-gray-700">Game Controls</h2>
            <div class="grid grid-cols-2 md:grid-cols-4 gap-3">
                <button onclick="sendCommand('start')" class="bg-green-600 hover:bg-green-700 text-white px-4 py-2 rounded transition duration-200" id="btnStart">
                    ▶️ Start
                </button>
                <button onclick="sendCommand('pause')" class="bg-yellow-600 hover:bg-yellow-700 text-white px-4 py-2 rounded transition duration-200" id="btnPause">
                    ⏸️ Pause
                </button>
                <button onclick="sendCommand('reset')" class="bg-gray-600 hover:bg-gray-700 text-white px-4 py-2 rounded transition duration-200">
                    🔄 Reset
                </button>
                <button onclick="sendCommand('addStrike')" class="bg-red-600 hover:bg-red-700 text-white px-4 py-2 rounded transition duration-200">
                    ⚠️ Add Strike
                </button>
                <button onclick="sendCommand('clearStrikes')" class="bg-blue-600 hover:bg-blue-700 text-white px-4 py-2 rounded transition duration-200">
                    🗑️ Clear Strikes
                </button>
                <button onclick="sendCommand('solveModule')" class="bg-purple-600 hover:bg-purple-700 text-white px-4 py-2 rounded transition duration-200">
                    ✅ Solve Module
                </button>
                <button onclick="openTimeModal()" class="bg-indigo-600 hover:bg-indigo-700 text-white px-4 py-2 rounded transition duration-200">
                    ⏱️ Set Time
                </button>
                <button onclick="openConfigModal()" class="bg-pink-600 hover:bg-pink-700 text-white px-4 py-2 rounded transition duration-200">
                    ⚙️ Config
                </button>
            </div>
        </div>

        <!-- Time Input Modal -->
        <div id="timeModal" class="fixed inset-0 bg-black bg-opacity-50 hidden items-center justify-center z-50">
            <div class="bg-gray-800 rounded-lg p-6 max-w-md">
                <h3 class="text-xl font-bold mb-4">Set Time Limit</h3>
                <input type="text" id="timeInput" placeholder="mm:ss" class="w-full bg-gray-700 border border-gray-600 rounded px-4 py-2 mb-4 text-white" value="5:00">
                <div class="flex gap-3">
                    <button onclick="setTime()" class="flex-1 bg-green-600 hover:bg-green-700 text-white px-4 py-2 rounded">Set</button>
                    <button onclick="closeTimeModal()" class="flex-1 bg-gray-600 hover:bg-gray-700 text-white px-4 py-2 rounded">Cancel</button>
                </div>
            </div>
        </div>

        <!-- Config Modal -->
        <div id="configModal" class="fixed inset-0 bg-black bg-opacity-50 hidden items-center justify-center z-50">
            <div class="bg-gray-800 rounded-lg p-6 max-w-md w-full max-h-[90vh] overflow-y-auto">
                <h3 class="text-xl font-bold mb-4">Configuration</h3>
                <div class="space-y-4">
                    <div>
                        <label class="block text-sm font-medium mb-2">Max Strikes</label>
                        <input type="number" id="maxStrikes" class="w-full bg-gray-700 border border-gray-600 rounded px-4 py-2 text-white" value="3">
                    </div>
                    <div>
                        <label class="flex items-center space-x-2">
                            <input type="checkbox" id="enableStrikeAccel" class="rounded">
                            <span>Enable Strike Acceleration</span>
                        </label>
                    </div>
                    <div>
                        <label class="block text-sm font-medium mb-2">Strike Acceleration Factor</label>
                        <input type="number" id="strikeAccelFactor" step="0.05" class="w-full bg-gray-700 border border-gray-600 rounded px-4 py-2 text-white" value="0.25">
                    </div>
                    <div>
                        <label class="flex items-center space-x-2">
                            <input type="checkbox" id="enableEmergencyAlarm" class="rounded">
                            <span>Enable Emergency Alarm</span>
                        </label>
                    </div>
                    <div>
                        <label class="block text-sm font-medium mb-2">Emergency Threshold (ms)</label>
                        <input type="number" id="emergencyThreshold" class="w-full bg-gray-700 border border-gray-600 rounded px-4 py-2 text-white" value="60000">
                    </div>
                    <div>
                        <label class="flex items-center space-x-2">
                            <input type="checkbox" id="enableNeedyModules" class="rounded">
                            <span>Enable Needy Modules</span>
                        </label>
                    </div>
                    <div>
                        <label class="flex items-center space-x-2">
                            <input type="checkbox" id="enableEdgework" class="rounded">
                            <span>Enable Edgework</span>
                        </label>
                    </div>
                    <div class="pt-4 border-t border-gray-700 flex gap-3">
                        <button onclick="saveConfig()" class="flex-1 bg-green-600 hover:bg-green-700 text-white px-4 py-2 rounded">Save</button>
                        <button onclick="closeConfigModal()" class="flex-1 bg-gray-600 hover:bg-gray-700 text-white px-4 py-2 rounded">Cancel</button>
                    </div>
                </div>
            </div>
        </div>

        <!-- Module Info -->
        <div class="grid grid-cols-1 md:grid-cols-2 gap-4 mb-6">
            <div class="bg-gray-800 rounded-lg p-6 border border-gray-700">
                <h2 class="text-xl font-bold mb-4 pb-2 border-b border-gray-700">Module Status</h2>
                <div class="space-y-2">
                    <div class="flex justify-between">
                        <span class="text-gray-400">Total Modules:</span>
                        <span class="font-bold" id="totalModules">0</span>
                    </div>
                    <div class="flex justify-between">
                        <span class="text-gray-400">Solved:</span>
                        <span class="font-bold text-green-500" id="solvedModules">0</span>
                    </div>
                    <div class="flex justify-between">
                        <span class="text-gray-400">Remaining:</span>
                        <span class="font-bold" id="remainingModules">0</span>
                    </div>
                </div>
            </div>

            <div class="bg-gray-800 rounded-lg p-6 border border-gray-700">
                <h2 class="text-xl font-bold mb-4 pb-2 border-b border-gray-700">Edgework</h2>
                <div class="space-y-2 text-sm">
                    <div class="flex justify-between">
                        <span class="text-gray-400">Serial:</span>
                        <span class="font-mono font-bold" id="serialNumber">-</span>
                    </div>
                    <div class="flex justify-between">
                        <span class="text-gray-400">Batteries:</span>
                        <span class="font-bold" id="batteries">0</span>
                    </div>
                    <div class="flex justify-between">
                        <span class="text-gray-400">Indicators:</span>
                        <span class="font-bold" id="indicators">0</span>
                    </div>
                    <div class="flex justify-between">
                        <span class="text-gray-400">Ports:</span>
                        <span class="font-bold" id="ports">0</span>
                    </div>
                </div>
            </div>
        </div>

        <!-- Connection Status -->
        <div class="bg-gray-800 rounded-lg p-4 border border-gray-700 text-center">
            <div class="flex items-center justify-center space-x-2">
                <div class="w-3 h-3 bg-green-500 rounded-full animate-pulse" id="connectionIndicator"></div>
                <span class="text-sm text-gray-400">Connected - Auto-refresh enabled</span>
            </div>
        </div>
    </div>

    <script>
        let config = {};

        function sendCommand(cmd) {
            fetch('/api/command', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({command: cmd})
            })
            .then(r => r.json())
            .then(data => {
                if (data.success) {
                    console.log('Command executed');
                    setTimeout(updateStatus, 100);
                } else {
                    alert('Error: ' + data.error);
                }
            })
            .catch(err => console.error('Error:', err));
        }

        function setTime() {
            const timeStr = document.getElementById('timeInput').value;
            sendCommand('setTime:' + timeStr);
            closeTimeModal();
        }

        function openTimeModal() {
            document.getElementById('timeModal').classList.remove('hidden');
            document.getElementById('timeModal').classList.add('flex');
        }

        function closeTimeModal() {
            document.getElementById('timeModal').classList.add('hidden');
            document.getElementById('timeModal').classList.remove('flex');
        }

        function openConfigModal() {
            fetch('/api/config')
                .then(r => r.json())
                .then(data => {
                    if (data.success) {
                        config = data.config;
                        document.getElementById('maxStrikes').value = config.maxStrikes;
                        document.getElementById('enableStrikeAccel').checked = config.enableStrikeAcceleration;
                        document.getElementById('strikeAccelFactor').value = config.strikeAccelerationFactor;
                        document.getElementById('enableEmergencyAlarm').checked = config.enableEmergencyAlarm;
                        document.getElementById('emergencyThreshold').value = config.emergencyAlarmThreshold;
                        document.getElementById('enableNeedyModules').checked = config.enableNeedyModules;
                        document.getElementById('enableEdgework').checked = config.enableEdgework;
                    }
                });
            document.getElementById('configModal').classList.remove('hidden');
            document.getElementById('configModal').classList.add('flex');
        }

        function closeConfigModal() {
            document.getElementById('configModal').classList.add('hidden');
            document.getElementById('configModal').classList.remove('flex');
        }

        function saveConfig() {
            config = {
                maxStrikes: parseInt(document.getElementById('maxStrikes').value),
                enableStrikeAcceleration: document.getElementById('enableStrikeAccel').checked,
                strikeAccelerationFactor: parseFloat(document.getElementById('strikeAccelFactor').value),
                enableEmergencyAlarm: document.getElementById('enableEmergencyAlarm').checked,
                emergencyAlarmThreshold: parseInt(document.getElementById('emergencyThreshold').value),
                enableNeedyModules: document.getElementById('enableNeedyModules').checked,
                enableEdgework: document.getElementById('enableEdgework').checked
            };

            fetch('/api/config', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify(config)
            })
            .then(r => r.json())
            .then(data => {
                if (data.success) {
                    closeConfigModal();
                    alert('Configuration saved!');
                } else {
                    alert('Error: ' + data.error);
                }
            });
        }

        function updateStatus() {
            fetch('/api/status')
                .then(r => r.json())
                .then(data => {
                    if (data.success) {
                        // Update game state
                        const stateNames = {
                            'IDLE': '🔵 Ready',
                            'RUNNING': '🟢 Running',
                            'PAUSED': '🟡 Paused',
                            'EXPLODED': '🔴 Exploded!',
                            'DEFUSED': '🟢 Defused!',
                            'VICTORY': '🎉 Victory!'
                        };
                        const stateName = stateNames[data.state] || data.state;
                        document.getElementById('gameState').textContent = stateName;
                        
                        // Update state card color
                        const stateCard = document.getElementById('stateCard');
                        stateCard.classList.remove('border-red-500', 'border-green-500', 'border-yellow-500', 'border-blue-500');
                        if (data.state === 'RUNNING') stateCard.classList.add('border-green-500');
                        else if (data.state === 'PAUSED') stateCard.classList.add('border-yellow-500');
                        else if (data.state === 'EXPLODED') stateCard.classList.add('border-red-500');
                        else stateCard.classList.add('border-blue-500');

                        // Update time
                        const timeMs = data.timeRemaining;
                        const minutes = Math.floor(timeMs / 60000);
                        const seconds = Math.floor((timeMs % 60000) / 1000);
                        document.getElementById('timeRemaining').textContent = 
                            `${minutes}:${seconds.toString().padStart(2, '0')}`;

                        // Update strikes
                        document.getElementById('strikes').textContent = `${data.strikes}/${data.maxStrikes}`;
                        
                        // Update strike card color
                        const strikeCard = document.getElementById('strikeCard');
                        strikeCard.classList.remove('border-red-500', 'border-yellow-500', 'border-green-500');
                        if (data.strikes >= data.maxStrikes) strikeCard.classList.add('border-red-500');
                        else if (data.strikes > 0) strikeCard.classList.add('border-yellow-500');
                        else strikeCard.classList.add('border-green-500');

                        // Update modules
                        document.getElementById('totalModules').textContent = data.totalModules;
                        document.getElementById('solvedModules').textContent = data.solvedModules;
                        document.getElementById('remainingModules').textContent = data.remainingModules;

                        // Update edgework
                        document.getElementById('serialNumber').textContent = data.serialNumber;
                        document.getElementById('batteries').textContent = data.batteries;
                        document.getElementById('indicators').textContent = data.indicators;
                        document.getElementById('ports').textContent = data.ports;

                        // Update buttons
                        const btnStart = document.getElementById('btnStart');
                        const btnPause = document.getElementById('btnPause');
                        if (data.state === 'IDLE') {
                            btnStart.disabled = false;
                            btnPause.disabled = true;
                        } else if (data.state === 'RUNNING') {
                            btnStart.disabled = true;
                            btnPause.disabled = false;
                        } else if (data.state === 'PAUSED') {
                            btnStart.disabled = false;
                            btnPause.disabled = true;
                        } else {
                            btnStart.disabled = true;
                            btnPause.disabled = true;
                        }
                    }
                })
                .catch(err => console.error('Error updating status:', err));
        }

        // Auto-refresh every 500ms
        setInterval(updateStatus, 500);
        
        // Initial update
        updateStatus();

        // Modal close on background click
        document.getElementById('timeModal').addEventListener('click', function(e) {
            if (e.target === this) closeTimeModal();
        });
        document.getElementById('configModal').addEventListener('click', function(e) {
            if (e.target === this) closeConfigModal();
        });
    </script>
</body>
</html>
)rawliteral";

void initWebServer(GameStateManager* gsm) {
    gameState = gsm;

    // Start WiFi AP
    Serial.print("Starting WiFi AP: ");
    Serial.println(ssid);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid, password);
    delay(100);
    
    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);

    // Create web server
    server = new WiFiServer(80);
    server->begin();
    Serial.println("Web server started on port 80");
}

void updateWebServer() {
    if (!server) return;
    
    WiFiClient client = server->available();
    
    if (client.available()) {
        String method, path;
        if (parseRequest(client, method, path)) {
            // Handle different routes
            if (method == "GET" && path == "/") {
                // Serve main HTML page
                sendResponse(client, 200, "text/html", String(html_page));
            } else if (method == "GET" && path == "/api/status") {
                handleStatus(client);
            } else if (method == "GET" && path == "/api/config") {
                handleGetConfig(client);
            } else if (method == "POST" && path == "/api/command") {
                // Read POST body
                String body = "";
                while (client.available()) {
                    body += (char)client.read();
                }
                handleCommand(client, body);
            } else if (method == "POST" && path == "/api/config") {
                // Read POST body
                String body = "";
                while (client.available()) {
                    body += (char)client.read();
                }
                handleSetConfig(client, body);
            } else {
                sendResponse(client, 404, "text/plain", "Not found");
            }
        }
        
        // Close connection after response
        delay(1);
        client.stop();
    }
}

String getWiFiIP() {
    if (WiFi.getMode() == WIFI_AP) {
        return WiFi.softAPIP().toString();
    } else {
        return WiFi.localIP().toString();
    }
}
