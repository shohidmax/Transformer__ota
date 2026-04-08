const express = require('express');
const http = require('http');
const { Server } = require('socket.io');
const path = require('path');

const app = express();
const server = http.createServer(app);
const io = new Server(server, { cors: { origin: "*" } });

// Middleware to parse JSON payloads
app.use(express.json());

// Serve static frontend files from the 'Dashboard' folder
app.use(express.static(path.join(__dirname, '../Dashboard')));

// ==========================================
// CONFIGURATION & PHONE VARIABLES
// ==========================================
const ESP32_IP = "http://192.168.0.201"; 
const POLL_INTERVAL = 1000; 
const PHONE_NUMBERS = ["+8801793496030", "+8801724958474"];
let lastSecurityState = 0;

async function triggerAlarmAPI() {
    console.log("🚨 ALARM TRIGGERED! Executing Server-based API calls...");
    const primaryUrl = "https://transformer.maxapi.esp32.site/api/broadcast";
    const backupUrl = "https://transformerv2.espserver.site/api/broadcast";
    
    const payload = {
        user_id: "user123",
        mac: "44:1D:64:BD:22:EC",
        phone: "Main Office",
        phone_call_list: PHONE_NUMBERS,
        payload: { address: "pole55", message: "Theft alarm detected " },
        response: []
    };

    try {
        let res = await fetch(primaryUrl, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(payload)
        });
        if (!res.ok) throw new Error("Primary API failed");
        console.log("✅ Primary API Call Success");
    } catch (e) {
        console.log("⚠️ Switching to Backup API...", e.message);
        try {
            let res2 = await fetch(backupUrl, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(payload)
            });
            if (!res2.ok) throw new Error("Backup API failed");
            console.log("✅ Backup API Call Success");
        } catch (err) {
            console.log("❌ Both APIs failed", err.message);
        }
    }
}

// ==========================================
// METHOD 1: POLL DATA FROM ESP32 (Auto-fetch)
// ==========================================
setInterval(async () => {
    try {
        const response = await fetch(`${ESP32_IP}/data`);
        if (response.ok) {
            const data = await response.json();
            data.isOnline = true; // Inject online flag
            
            // Check if alarm triggered (Transition from safe/warning to ALARM)
            if (data.state === 2 && lastSecurityState !== 2) {
                triggerAlarmAPI();
            }
            lastSecurityState = data.state;

            // Broadcast the real-time data
            io.emit('sensorUpdate', data);
        }
    } catch (error) {
        // ESP32 is offline or unreachable
        io.emit('sensorUpdate', { isOnline: false });
    }
}, POLL_INTERVAL);

// ==========================================
// METHOD 2: RECEIVE DATA FROM ESP32 (Webhook)
// If you modify ESP32 to push data, it can send POST requests here
// ==========================================
app.post('/api/push-data', (req, res) => {
    const data = req.body;
    console.log('Received Push from ESP32:', data);
    
    // Broadcast data to all connected web clients instantly
    io.emit('sensorUpdate', data);
    res.status(200).json({ status: "success" });
});

// ==========================================
// SOCKET.IO CONNECTION HANDLER
// ==========================================
io.on('connection', (socket) => {
    console.log(`🟢 New Web Client Connected: ${socket.id}`);
    
    socket.on('disconnect', () => {
        console.log(`🔴 Web Client Disconnected: ${socket.id}`);
    });
});

// Start the Node.js Server
const PORT = process.env.PORT || 3000;
server.listen(PORT, () => {
    console.log(`=================================================`);
    console.log(`🚀 Real-time Monitoring Server is running!`);
    console.log(`👉 Open your browser and go to: http://localhost:${PORT}`);
    console.log(`=================================================`);
});