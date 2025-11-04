#include "web_html.h"

// HTML page with Tailwind CSS - includes debug section
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
    <div class="container mx-auto px-4 py-8 max-w-6xl">
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
                <button onclick="pingAllModules()" class="bg-teal-600 hover:bg-teal-700 text-white px-4 py-2 rounded transition duration-200" title="Modules discover automatically via heartbeat">
                    📡 Discovery Info
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

        <!-- Module Grid -->
        <div class="bg-gray-800 rounded-lg p-6 border border-gray-700 mb-6">
            <h2 class="text-xl font-bold mb-4 pb-2 border-b border-gray-700">Connected Modules</h2>
            <div id="moduleGrid" class="grid grid-cols-2 md:grid-cols-3 lg:grid-cols-4 gap-4">
                <!-- Modules will be populated here -->
            </div>
        </div>

        <!-- CAN Log Section -->
        <div class="bg-gray-800 rounded-lg p-6 border border-gray-700 mb-6">
            <div class="flex justify-between items-center mb-4 pb-2 border-b border-gray-700">
                <h2 class="text-xl font-bold">CAN Bus Log (<span id="canLogCount">0</span> messages)</h2>
                <button onclick="updateCanLog()" class="bg-cyan-600 hover:bg-cyan-700 text-white px-4 py-2 rounded text-sm">
                    🔄 Refresh
                </button>
            </div>
            <div class="overflow-x-auto">
                <table class="w-full text-sm">
                    <thead>
                        <tr class="border-b border-gray-600">
                            <th class="text-left py-2 px-4">Time</th>
                            <th class="text-left py-2 px-4">Sender</th>
                            <th class="text-left py-2 px-4">Receiver</th>
                            <th class="text-left py-2 px-4">Data</th>
                        </tr>
                    </thead>
                    <tbody id="canLogTableBody">
                        <!-- Messages will be populated here -->
                    </tbody>
                </table>
            </div>
        </div>

        <!-- Connection Status -->
        <div class="bg-gray-800 rounded-lg p-4 border border-gray-700 text-center">
            <div class="flex items-center justify-center space-x-2">
                <div class="w-3 h-3 bg-green-500 rounded-full animate-pulse" id="connectionIndicator"></div>
                <span class="text-sm text-gray-400">Connected - Auto-refresh every 5 seconds</span>
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
                    setTimeout(updateAll, 100);
                } else {
                    alert('Error: ' + data.error);
                }
            })
            .catch(err => console.error('Error:', err));
        }

        function pingAllModules() {
            fetch('/api/ping', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'}
            })
            .then(r => r.json())
            .then(data => {
                if (data.success) {
                    console.log('Discovery info retrieved');
                    alert(data.message || 'Modules discover automatically via heartbeat when game is not running');
                    setTimeout(updateAll, 500);
                } else {
                    alert('Error: ' + (data.error || 'Unknown error'));
                }
            })
            .catch(err => {
                console.error('Error:', err);
                alert('Error getting discovery info');
            });
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

        // Combined update function that fetches all data in one request
        function updateAll() {
            fetch('/api/all')
                .then(r => r.json())
                .then(data => {
                    if (data.success) {
                        // Update status data
                        const status = data.status;
                        const stateNames = {
                            'IDLE': '🔵 Ready',
                            'RUNNING': '🟢 Running',
                            'PAUSED': '🟡 Paused',
                            'EXPLODED': '🔴 Exploded!',
                            'DEFUSED': '🟢 Defused!',
                            'VICTORY': '🎉 Victory!'
                        };
                        const stateName = stateNames[status.state] || status.state;
                        document.getElementById('gameState').textContent = stateName;
                        
                        // Update state card color
                        const stateCard = document.getElementById('stateCard');
                        stateCard.classList.remove('border-red-500', 'border-green-500', 'border-yellow-500', 'border-blue-500');
                        if (status.state === 'RUNNING') stateCard.classList.add('border-green-500');
                        else if (status.state === 'PAUSED') stateCard.classList.add('border-yellow-500');
                        else if (status.state === 'EXPLODED') stateCard.classList.add('border-red-500');
                        else stateCard.classList.add('border-blue-500');

                        // Update time
                        const timeMs = status.timeRemaining;
                        const minutes = Math.floor(timeMs / 60000);
                        const seconds = Math.floor((timeMs % 60000) / 1000);
                        document.getElementById('timeRemaining').textContent = 
                            `${minutes}:${seconds.toString().padStart(2, '0')}`;

                        // Update strikes
                        document.getElementById('strikes').textContent = `${status.strikes}/${status.maxStrikes}`;
                        
                        // Update strike card color
                        const strikeCard = document.getElementById('strikeCard');
                        strikeCard.classList.remove('border-red-500', 'border-yellow-500', 'border-green-500');
                        if (status.strikes >= status.maxStrikes) strikeCard.classList.add('border-red-500');
                        else if (status.strikes > 0) strikeCard.classList.add('border-yellow-500');
                        else strikeCard.classList.add('border-green-500');

                        // Update modules summary
                        document.getElementById('totalModules').textContent = status.totalModules;
                        document.getElementById('solvedModules').textContent = status.solvedModules;
                        document.getElementById('remainingModules').textContent = status.remainingModules;

                        // Update edgework
                        document.getElementById('serialNumber').textContent = status.serialNumber;
                        document.getElementById('batteries').textContent = status.batteries;
                        document.getElementById('indicators').textContent = status.indicators;
                        document.getElementById('ports').textContent = status.ports;

                        // Update modules grid
                        const grid = document.getElementById('moduleGrid');
                        grid.innerHTML = '';
                        
                        if (data.modules && data.modules.length === 0) {
                            grid.innerHTML = '<div class="col-span-full text-center text-gray-400">No modules connected</div>';
                        } else if (data.modules) {
                            data.modules.forEach(module => {
                                const card = document.createElement('div');
                                card.className = 'bg-gray-700 rounded-lg p-4 border-2';
                                
                                // Determine border color based on status
                                if (module.isSolved) {
                                    card.classList.add('border-green-500');
                                } else if (!module.isActive) {
                                    card.classList.add('border-red-500');
                                } else if (module.isRegistered) {
                                    card.classList.add('border-blue-500');
                                } else {
                                    card.classList.add('border-yellow-500');
                                }
                                
                                const statusIcon = module.isSolved ? '✅' : (!module.isActive ? '❌' : (module.isRegistered ? '🔵' : '🟡'));
                                const statusText = module.isSolved ? 'Solved' : (!module.isActive ? 'Offline' : (module.isRegistered ? 'Registered' : 'Discovered'));
                                
                                card.innerHTML = `
                                    <div class="font-bold text-lg mb-2">${statusIcon} ${module.type}</div>
                                    <div class="text-sm text-gray-400 mb-1">ID: ${module.id}</div>
                                    <div class="text-xs text-gray-500">Status: ${statusText}</div>
                                    ${module.progress > 0 ? `<div class="mt-2 bg-gray-600 rounded-full h-2"><div class="bg-blue-500 h-2 rounded-full" style="width: ${module.progress}%"></div></div>` : ''}
                                `;
                                
                                grid.appendChild(card);
                            });
                        }

                        // Update CAN log summary (only recent messages)
                        if (data.canLog) {
                            document.getElementById('canLogCount').textContent = data.canLog.count;
                            const tbody = document.getElementById('canLogTableBody');
                            tbody.innerHTML = '';
                            
                            if (data.canLog.recentMessages && data.canLog.recentMessages.length > 0) {
                                data.canLog.recentMessages.forEach((msg, idx) => {
                                    const row = document.createElement('tr');
                                    row.className = idx % 2 === 0 ? 'bg-gray-700' : 'bg-gray-800';
                                    
                                    // Format data as hex
                                    const dataStr = msg.data.map(b => '0x' + b.toString(16).padStart(2, '0').toUpperCase()).join(' ');
                                    
                                    row.innerHTML = `
                                        <td class="py-2 px-4 font-mono text-xs">${msg.timestamp}ms</td>
                                        <td class="py-2 px-4 font-mono text-xs">0x${msg.senderId.toString(16).toUpperCase()}</td>
                                        <td class="py-2 px-4 font-mono text-xs">0x${msg.receiverId.toString(16).toUpperCase()}</td>
                                        <td class="py-2 px-4 font-mono text-xs">${dataStr}</td>
                                    `;
                                    tbody.appendChild(row);
                                });
                            }
                        }

                        // Update button states
                        updateButtons(status.state);
                    }
                })
                .catch(err => console.error('Error updating all data:', err));
        }

        // Legacy functions for manual refresh buttons (can still use individual endpoints if needed)
        function updateStatus() {
            updateAll(); // Just call the combined update
        }

        // Legacy functions for manual refresh buttons - use full CAN log endpoint
        function updateCanLog() {
            fetch('/api/debug')
                .then(r => r.json())
                .then(data => {
                    if (data.success) {
                        document.getElementById('canLogCount').textContent = data.count;
                        const tbody = document.getElementById('canLogTableBody');
                        tbody.innerHTML = '';
                        
                        data.messages.forEach((msg, idx) => {
                            const row = document.createElement('tr');
                            row.className = idx % 2 === 0 ? 'bg-gray-700' : 'bg-gray-800';
                            
                            // Show decoded data if available, otherwise hex
                            const dataStr = msg.dataDecoded || msg.data.map(b => '0x' + b.toString(16).padStart(2, '0').toUpperCase()).join(' ');
                            
                            row.innerHTML = `
                                <td class="py-2 px-4 font-mono text-xs">${msg.timestamp}ms</td>
                                <td class="py-2 px-4">${msg.senderDisplay}</td>
                                <td class="py-2 px-4">${msg.receiverDisplay}</td>
                                <td class="py-2 px-4 font-mono text-xs">${dataStr}</td>
                            `;
                            tbody.appendChild(row);
                        });
                    }
                })
                .catch(err => {
                    console.error('Error fetching CAN log:', err);
                });
        }

        function updateModules() {
            updateAll(); // Just call the combined update
        }

        // Update button states
        function updateButtons(state) {
            const btnStart = document.getElementById('btnStart');
            const btnPause = document.getElementById('btnPause');
            if (state === 'IDLE') {
                btnStart.disabled = false;
                btnPause.disabled = true;
            } else if (state === 'RUNNING') {
                btnStart.disabled = true;
                btnPause.disabled = false;
            } else if (state === 'PAUSED') {
                btnStart.disabled = false;
                btnPause.disabled = true;
            } else {
                btnStart.disabled = true;
                btnPause.disabled = true;
            }
        }

        // Auto-refresh every 5 seconds using combined endpoint
        setInterval(updateAll, 5000);
        
        // Initial update
        updateAll();

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

