const express = require('express');
const http = require('http');
const { Server } = require('socket.io');
const path = require('path');

const app = express();
const server = http.createServer(app);
const io = new Server(server);

// Middleware to parse JSON payloads
app.use(express.json());

// Serve static frontend files from the 'Dashboard' folder
app.use(express.static(path.join(__dirname, '../Dashboard')));

// ==========================================
// METHOD 1: POLL DATA FROM ESP32 (Auto-fetch)
// Node.js will automatically fetch data from ESP32 every 1 second
// ==========================================
const ESP32_IP = "http://192.168.0.201"; // ⚠️ Change this to your ESP32's current IP
const POLL_INTERVAL = 1000; // 1 second

setInterval(async () => {
    try {
        // Only works if using Node.js v18+ (uses native fetch)
        const response = await fetch(`${ESP32_IP}/data`);
        if (response.ok) {
            const data = await response.json();
            // Broadcast the real-time data to all connected web clients
            io.emit('sensorUpdate', data);
        }
    } catch (error) {
        // Silently fail if ESP32 is offline to prevent console spam
        // console.error(`ESP32 Offline: ${error.message}`);
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