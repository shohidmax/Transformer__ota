const express = require('express');
const http = require('http');
const { Server } = require('socket.io');
const path = require('path');
const fs = require('fs');

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
let PHONE_NUMBERS = ["+8801793496030", "+8801724958474"];
const NUMBERS_FILE = path.join(__dirname, 'numbers.json');

try {
    if (fs.existsSync(NUMBERS_FILE)) {
        PHONE_NUMBERS = JSON.parse(fs.readFileSync(NUMBERS_FILE));
    }
} catch (e) {
    console.log("Error loading numbers.json:", e.message);
}
let lastSecurityState = 0;
let lastBothRadarsTriggered = false;

async function triggerAlarmAPI() {
    console.log("🚨 ALARM TRIGGERED! Executing Server-based API calls...");
    const primaryUrl = "https://800lcall.espserver.site/api/broadcast";
    const backupUrl = "https://sim800l.maxapi.esp32.site/api/broadcast";
    
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
// RECEIVE DATA FROM ESP32 (Webhook Push)
// ==========================================
let onlineTimeout = null;
let pendingCommand = null;

const handleOffline = () => {
    io.emit('sensorUpdate', { isOnline: false });
};

// Auto-trigger offline on boot if no ESP connects
onlineTimeout = setTimeout(handleOffline, 5000);

app.post('/api/push-data', (req, res) => {
    const data = req.body;
    data.isOnline = true;
    
    // Instant API logic: when both radars detect, call API immediately
    let bothRadarsNow = (data.rdr1 == 1 && data.rdr2 == 1);
    if (bothRadarsNow && !lastBothRadarsTriggered) {
        triggerAlarmAPI();
    }
    lastBothRadarsTriggered = bothRadarsNow;
    
    lastSecurityState = data.state;

    // Reset offline timeout
    if (onlineTimeout) clearTimeout(onlineTimeout);
    onlineTimeout = setTimeout(handleOffline, 5000); // 5 sec without push = offline
    
    // Broadcast data to all connected web clients instantly
    io.emit('sensorUpdate', data);
    
    let responseJson = { status: "success" };
    if (pendingCommand) {
        responseJson.command = pendingCommand;
        pendingCommand = null;
    }
    res.status(200).json(responseJson);
});

// ==========================================
// SOCKET.IO CONNECTION HANDLER
// ==========================================
io.on('connection', (socket) => {
    console.log(`🟢 New Web Client Connected: ${socket.id}`);
    
    socket.on('disconnect', () => {
        console.log(`🔴 Web Client Disconnected: ${socket.id}`);
    });

    // Send the current phone numbers array when client connects
    socket.emit('initNumbers', PHONE_NUMBERS);
    
    // Listen for phone number updates from dashboard
    socket.on('updateNumbers', (newNumbers) => {
        PHONE_NUMBERS = newNumbers;
        fs.writeFileSync(NUMBERS_FILE, JSON.stringify(PHONE_NUMBERS));
        io.emit('initNumbers', PHONE_NUMBERS); // Sync across all open dashboards
        console.log("Phone numbers updated:", PHONE_NUMBERS);
    });

    // Listen for manual ESP Reboot trigger from dashboard
    socket.on('rebootESP', () => {
        pendingCommand = "restart";
        console.log("Reboot command queued for ESP32...");
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