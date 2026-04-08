/*
  Transformer Security Node - Ultimate Version
  ------------------------------------------------
  Features:
  - Instant API Call on Radar Detection
  - Radar 10-Second Continuous Detection Logic
  - Premium Local Web Dashboard
  - Environment Monitoring (DHT11)
  - GitHub OTA Updates & WiFiManager
  - OLED Live Status Display

  Pins:
  - PIR       : GPIO 10
  - RADAR 1   : GPIO 4
  - RADAR 2   : GPIO 5
  - RELAY     : GPIO 2
  - BUZZER    : GPIO 1
  - DHT11     : GPIO 7
  - OLED SDA  : GPIO 8
  - OLED SCL  : GPIO 9
*/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <Update.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h> 

// --- Pin Definitions ---
#define PIR_PIN 10
#define RADAR_1_PIN 4
#define RADAR_2_PIN 5
#define ALARM_RELAY_PIN 2
#define BUZZER_PIN 1 
#define DHT_PIN 7         

#define I2C_SDA 8
#define I2C_SCL 9
#define DHTTYPE DHT11     

// --- OLED Configuration ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --- DHT Setup ---
DHT dht(DHT_PIN, DHTTYPE);

// --- WebServer Setup ---
WebServer server(80);

// --- OTA Configuration ---
const char* firmwareUrl = "https://github.com/shohidmax/Transformer_ota/releases/download/transformerq/Transfer_Sec.ino.bin";
const char* versionUrl = "https://raw.githubusercontent.com/shohidmax/Transformer_ota/refs/heads/main/Esp32_C5/virsion.txt";
const char* currentFirmwareVersion = "1.1.2";
const unsigned long updateCheckInterval = 5 * 60 * 1000; // 5 minutes
unsigned long lastUpdateCheck = 0;

// --- State Variables ---
float humidity = 0.0;
float temperature = 0.0;
bool pirState = false;
bool radar1State = false;
bool radar2State = false;
int securityState = 0; // 0 = Safe, 1 = Warning, 2 = Alarm

unsigned long previousMillis = 0;
const long interval = 2000; // 2 sec for DHT and OLED update

// --- Radar 10-Second Timer Variable ---
unsigned long radarDetectStartTime = 0;

// --- Function Prototypes ---
void connectToWiFi();
void checkForFirmwareUpdate();
String fetchLatestVersion();
void downloadAndApplyFirmware();
bool startOTAUpdate(WiFiClient* client, int contentLength);

// --- HTML Premium Dashboard ---
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>Smart Security Dashboard</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    :root { --bg: #0f172a; --card: #1e293b; --text: #f8fafc; --alert: #ef4444; --warning: #f59e0b; --safe: #10b981; --accent: #38bdf8; }
    body { background-color: var(--bg); color: var(--text); font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; margin: 0; padding: 20px; display: flex; justify-content: center; }
    .container { max-width: 800px; width: 100%; }
    .header { text-align: center; margin-bottom: 30px; padding-bottom: 15px; border-bottom: 1px solid rgba(255,255,255,0.1); }
    .header h1 { color: var(--accent); margin: 0; font-size: 2.2rem; letter-spacing: 1px; }
    .header p { color: #94a3b8; margin: 5px 0 0 0; font-size: 0.95rem; }
    
    #statusCard { border: 2px solid rgba(16, 185, 129, 0.3); background-color: rgba(16, 185, 129, 0.05); text-align: center; transition: all 0.3s ease; }
    #sysStatusText { margin: 0; font-size: 1.8rem; color: var(--safe); transition: all 0.3s ease; }
    
    .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(320px, 1fr)); gap: 25px; margin-top: 25px; }
    .card { background-color: var(--card); border-radius: 16px; padding: 25px; box-shadow: 0 10px 25px rgba(0,0,0,0.5); border: 1px solid rgba(255,255,255,0.05); }
    .card h2 { margin-top: 0; color: #cbd5e1; font-size: 1.3rem; display: flex; align-items: center; gap: 10px; border-bottom: 1px solid rgba(255,255,255,0.05); padding-bottom: 15px; margin-bottom: 15px; }
    .sensor-row { display: flex; justify-content: space-between; align-items: center; padding: 14px 0; border-bottom: 1px solid rgba(255,255,255,0.02); }
    .sensor-row:last-child { border-bottom: none; padding-bottom: 0; }
    .sensor-name { font-size: 1.1rem; color: #e2e8f0; }
    
    .badge { padding: 6px 16px; border-radius: 20px; font-weight: 600; font-size: 0.9rem; letter-spacing: 0.5px; transition: all 0.3s ease; }
    .badge.safe { background-color: rgba(16, 185, 129, 0.15); color: var(--safe); border: 1px solid rgba(16, 185, 129, 0.3); }
    .badge.alert { background-color: rgba(239, 68, 68, 0.15); color: var(--alert); border: 1px solid rgba(239, 68, 68, 0.3); box-shadow: 0 0 15px rgba(239, 68, 68, 0.4); animation: pulse 1.5s infinite; }
    
    .env-data { display: flex; justify-content: space-around; text-align: center; padding-top: 15px; }
    .env-box { display: flex; flex-direction: column; align-items: center; background: rgba(0,0,0,0.2); padding: 15px; border-radius: 12px; width: 40%; }
    .env-value { font-size: 2.5rem; font-weight: bold; color: var(--accent); }
    .env-unit { font-size: 1rem; color: #94a3b8; margin-left: 5px; }
    .env-label { font-size: 0.9rem; color: #cbd5e1; margin-top: 8px; text-transform: uppercase; letter-spacing: 1px; font-size: 0.8rem; }
    
    @keyframes pulse { 0% { box-shadow: 0 0 0 0 rgba(239, 68, 68, 0.6); } 70% { box-shadow: 0 0 0 12px rgba(239, 68, 68, 0); } 100% { box-shadow: 0 0 0 0 rgba(239, 68, 68, 0); } }
    @keyframes pulse-warn { 0% { box-shadow: 0 0 0 0 rgba(245, 158, 11, 0.6); } 70% { box-shadow: 0 0 0 12px rgba(245, 158, 11, 0); } 100% { box-shadow: 0 0 0 0 rgba(245, 158, 11, 0); } }
  </style>
</head>
<body>
  <div class="container">
    <div class="header">
      <h1>🛡️ Transformer Security</h1>
      <p>Real-time Monitoring & OTA System</p>
    </div>
    
    <div class="card" id="statusCard">
      <h2 id="sysStatusText">✅ SYSTEM SAFE</h2>
    </div>

    <div class="grid">
      <div class="card">
        <h2>🌡️ Environment Monitor</h2>
        <div class="env-data">
          <div class="env-box">
            <div><span class="env-value" id="temp">--</span><span class="env-unit">&deg;C</span></div>
            <div class="env-label">Temperature</div>
          </div>
          <div class="env-box">
            <div><span class="env-value" id="hum">--</span><span class="env-unit">%</span></div>
            <div class="env-label">Humidity</div>
          </div>
        </div>
      </div>
      <div class="card">
        <h2>🏃‍♂️ Sensor Status</h2>
        <div class="sensor-row">
          <span class="sensor-name">📡 PIR (Long Range)</span>
          <span id="pir" class="badge safe">CLEAR</span>
        </div>
        <div class="sensor-row">
          <span class="sensor-name">🎯 Radar 1 (Left)</span>
          <span id="rdr1" class="badge safe">CLEAR</span>
        </div>
        <div class="sensor-row">
          <span class="sensor-name">🎯 Radar 2 (Right)</span>
          <span id="rdr2" class="badge safe">CLEAR</span>
        </div>
      </div>
    </div>
  </div>
  <script>
    setInterval(function() {
      fetch('/data').then(response => response.json()).then(data => {
        document.getElementById("temp").innerText = isNaN(data.temp) ? "Err" : data.temp.toFixed(1);
        document.getElementById("hum").innerText = isNaN(data.hum) ? "Err" : data.hum.toFixed(0);
        
        const updateUI = (id, state) => {
          const el = document.getElementById(id);
          if(state) {
            el.innerText = "DETECTED";
            el.className = "badge alert";
          } else {
            el.innerText = "CLEAR";
            el.className = "badge safe";
          }
        };
        
        updateUI("pir", data.pir);
        updateUI("rdr1", data.rdr1);
        updateUI("rdr2", data.rdr2);

        // Update System Status Card
        const statCard = document.getElementById("statusCard");
        const statText = document.getElementById("sysStatusText");
        
        if (data.state === 2) {
            statCard.style.borderColor = "var(--alert)";
            statCard.style.backgroundColor = "rgba(239, 68, 68, 0.05)";
            statCard.style.boxShadow = "0 0 20px rgba(239, 68, 68, 0.3)";
            statText.style.color = "var(--alert)";
            statText.innerText = "🚨 ALARM ACTIVE!";
        } else if (data.state === 1) {
            statCard.style.borderColor = "var(--warning)";
            statCard.style.backgroundColor = "rgba(245, 158, 11, 0.05)";
            statCard.style.boxShadow = "0 0 20px rgba(245, 158, 11, 0.3)";
            statText.style.color = "var(--warning)";
            statText.innerText = "⚠️ WARNING (MOTION)";
        } else {
            statCard.style.borderColor = "rgba(16, 185, 129, 0.3)";
            statCard.style.backgroundColor = "rgba(16, 185, 129, 0.05)";
            statCard.style.boxShadow = "none";
            statText.style.color = "var(--safe)";
            statText.innerText = "✅ SYSTEM SAFE";
        }

      }).catch(error => console.log("Network error"));
    }, 1000); 
  </script>
</body>
</html>
)rawliteral";

// --- Web Server Handlers ---
void handleRoot() {
  server.send(200, "text/html", index_html);
}

void handleData() {
  String json = "{";
  json += "\"temp\":" + String(temperature) + ",";
  json += "\"hum\":" + String(humidity) + ",";
  json += "\"pir\":" + String(pirState) + ",";
  json += "\"rdr1\":" + String(radar1State) + ",";
  json += "\"rdr2\":" + String(radar2State) + ",";
  json += "\"state\":" + String(securityState) + ",";
  json += "\"wifi_ssid\":\"" + WiFi.SSID() + "\",";
  json += "\"wifi_rssi\":" + String(WiFi.RSSI()) + ",";
  json += "\"wifi_ip\":\"" + WiFi.localIP().toString() + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // Initialize Pins
  pinMode(PIR_PIN, INPUT);
  pinMode(RADAR_1_PIN, INPUT);
  pinMode(RADAR_2_PIN, INPUT);
  
  pinMode(ALARM_RELAY_PIN, OUTPUT);
  digitalWrite(ALARM_RELAY_PIN, LOW);
  
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // Boot Test 
  digitalWrite(ALARM_RELAY_PIN, HIGH);
  digitalWrite(BUZZER_PIN, HIGH);
  delay(300);
  digitalWrite(ALARM_RELAY_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  // Initialize Sensors
  dht.begin();
  Wire.begin(I2C_SDA, I2C_SCL);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("OLED Failed!"));
    for(;;);
  }
  
  // Setup WiFi
  connectToWiFi();
  
  // Check for OTA Updates on boot
  if (WiFi.status() == WL_CONNECTED) {
    checkForFirmwareUpdate();
  }

  // Start Web Server
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.begin();
  Serial.println(F("HTTP server started"));
}

void loop() {
  // 1. Web Server Handling
  server.handleClient();

  // 2. Read Motion & Presence Sensors Instantly
  pirState = digitalRead(PIR_PIN);
  radar1State = digitalRead(RADAR_1_PIN);
  radar2State = digitalRead(RADAR_2_PIN);

  // 3. Security Logic Evaluation (10-Sec Continuous Radar Check & Instant API)
  if (radar1State || radar2State) {
    
    // Start the timer if it's the first time detecting
    if (radarDetectStartTime == 0) {
      radarDetectStartTime = millis(); 
    }

    // Check if 10 seconds (10000 ms) have passed continuously
    if (millis() - radarDetectStartTime >= 10000) {
      securityState = 2; // ALARM!
      digitalWrite(ALARM_RELAY_PIN, HIGH); // TURN ON SIREN!
    } else {
      securityState = 1; // Treat as WARNING while counting down to 10s
      digitalWrite(ALARM_RELAY_PIN, LOW); // Keep siren off
    }
    
  } else {
    // Reset the timer immediately if target clears even for a moment
    radarDetectStartTime = 0; 

    // Fallback to check PIR if radars are clear
    if (pirState) {
      securityState = 1; // WARNING
      digitalWrite(ALARM_RELAY_PIN, LOW); // Keep siren off
    } else {
      securityState = 0; // SAFE
      digitalWrite(ALARM_RELAY_PIN, LOW); // Siren off
    }
  }

  // 4. Instant Buzzer Logic
  if (securityState == 2) {
    // Fast beep beep (ALARM Active)
    if (millis() % 300 < 150) digitalWrite(BUZZER_PIN, HIGH);
    else digitalWrite(BUZZER_PIN, LOW);
  } else if (securityState == 1) {
    // Slow warning beep (Motion detected / 10s radar countdown)
    if (millis() % 1000 < 100) digitalWrite(BUZZER_PIN, HIGH);
    else digitalWrite(BUZZER_PIN, LOW);
  } else {
    // Safe
    digitalWrite(BUZZER_PIN, LOW);
  }

  // 5. Non-blocking Timer for DHT11 & OLED (2 seconds)
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    // Read DHT11
    humidity = dht.readHumidity();
    temperature = dht.readTemperature();

    // Update OLED Display
    updateDisplay();
    
    // Push Data to Node.js Server
    pushDataToServer();
  }

  // 6. Non-blocking Timer for OTA Checking (5 minutes)
  if (currentMillis - lastUpdateCheck >= updateCheckInterval) {
    lastUpdateCheck = currentMillis;
    if (WiFi.status() == WL_CONNECTED) {
      checkForFirmwareUpdate();
    } else {
      WiFi.reconnect();
    }
  }

  // CRITICAL FIX: Give the single-core CPU time to breathe and feed the Watchdog
  delay(10); 
}

void updateDisplay() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  
  // Header with IP
  display.setTextSize(1);
  display.setCursor(0, 0);
  if(WiFi.status() == WL_CONNECTED) {
    display.print(WiFi.localIP()); 
  } else {
    display.print(F("No WiFi"));
  }
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

  // Security Status Text
  display.setCursor(0, 15);
  if (securityState == 2) {
    display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    display.print(F(" ALARM ACTIVE! "));
    display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
  } else if (securityState == 1) {
    display.print(F(" WARNING: MOTION "));
  } else {
    display.print(F(" SYSTEM SAFE "));
  }

  // DHT11 Data
  display.setCursor(0, 30);
  display.print(F("T: ")); 
  if (isnan(temperature)) display.print(F("Err"));
  else { display.print(temperature, 1); display.print(F("C")); }
  
  display.setCursor(64, 30);
  display.print(F("H: "));
  if (isnan(humidity)) display.print(F("Err"));
  else { display.print(humidity, 0); display.print(F("%")); }

  // Radar Data
  display.setCursor(0, 45);
  display.print(F("RDR1: "));
  display.println(radar1State ? F("DET!") : F("CLR"));
  
  display.setCursor(64, 45);
  display.print(F("RDR2: "));
  display.println(radar2State ? F("DET!") : F("CLR"));

  display.display();
}

// ==========================================
//             PUSH DATA TO SERVER API
// ==========================================
void pushDataToServer() {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure secureClient;
    secureClient.setInsecure();
    HTTPClient http;
    http.setTimeout(4000); // 4-second timeout to avoid loop hanging
    
    // Webhook destination (Node.js Server)
    String serverUrl = "https://transformer.maxapi.esp32.site/api/push-data"; 
    
    // Build JSON exactly like local handleData()
    String json = "{";
    json += "\"temp\":" + String(temperature) + ",";
    json += "\"hum\":" + String(humidity) + ",";
    json += "\"pir\":" + String(pirState) + ",";
    json += "\"rdr1\":" + String(radar1State) + ",";
    json += "\"rdr2\":" + String(radar2State) + ",";
    json += "\"state\":" + String(securityState) + ",";
    json += "\"wifi_ssid\":\"" + WiFi.SSID() + "\",";
    json += "\"wifi_rssi\":" + String(WiFi.RSSI()) + ",";
    json += "\"wifi_ip\":\"" + WiFi.localIP().toString() + "\"";
    json += "}";

    http.begin(secureClient, serverUrl);
    http.addHeader("Content-Type", "application/json");
    http.POST(json);
    http.end();
  }
}

// ==========================================
//             API CALL FUNCTION (Moved to Server)
// ==========================================

// ==========================================
//             OTA & WIFI FUNCTIONS
// ==========================================

void connectToWiFi() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 10);
  display.println(F("Connecting to WiFi.."));
  display.println(F("Or connect to AP:"));
  display.println(F("'Transformer_AP'"));
  display.display();

  Serial.println("Starting WiFiManager...");
  WiFiManager wm;
  wm.setConfigPortalTimeout(180);
  bool res = wm.autoConnect("Transformer_AP", "admin123");
  
  display.clearDisplay();
  display.setCursor(0, 10);
  if (!res) {
    display.println(F("WiFi Timeout!"));
    display.println(F("Running Offline."));
  } else {
    display.println(F("WiFi Connected!"));
    display.print(WiFi.localIP());
  }
  display.display();
  delay(2000);
}

void checkForFirmwareUpdate() {
  Serial.println("Checking for firmware update...");
  Serial.flush(); // Ensure the print goes out before the heavy SSL handshake starts

  String latestVersion = fetchLatestVersion();
  if (latestVersion == "" || latestVersion == currentFirmwareVersion) return;

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 10);
  display.println(F("NEW OTA FOUND!"));
  display.println(F("Downloading..."));
  display.display();
  
  downloadAndApplyFirmware();
}

String fetchLatestVersion() {
  WiFiClientSecure client;
  client.setInsecure(); 
  client.setTimeout(15000); 
  
  HTTPClient http;
  http.setTimeout(15000); 

  if (!http.begin(client, versionUrl)) return "";

  int httpCode = http.GET();
  String latestVersion = "";
  if (httpCode == HTTP_CODE_OK) {
    latestVersion = http.getString();
    latestVersion.trim(); 
  }
  http.end();
  return latestVersion;
}

void downloadAndApplyFirmware() {
  WiFiClientSecure client;
  client.setInsecure(); 
  client.setTimeout(15000); 

  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(15000); 
  
  if (!http.begin(client, firmwareUrl)) return;

  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    int contentLength = http.getSize();
    if (contentLength > 0) {
      WiFiClient* stream = http.getStreamPtr();
      if (startOTAUpdate(stream, contentLength)) {
        display.clearDisplay();
        display.setCursor(0, 20);
        display.println(F("UPDATE SUCCESS!"));
        display.println(F("Rebooting..."));
        display.display();
        delay(1000);
        ESP.restart();
      }
    }
  }
  http.end();
}

bool startOTAUpdate(WiFiClient* client, int contentLength) {
  if (!Update.begin(contentLength)) return false;

  size_t written = 0;
  int progress = 0;
  int lastProgress = -1;
  uint8_t buffer[1024]; 
  const unsigned long timeoutDuration = 20000; 
  unsigned long lastDataTime = millis();

  while (written < contentLength) {
    if (client->available()) {
      int bytesRead = client->read(buffer, sizeof(buffer));
      if (bytesRead > 0) {
        Update.write(buffer, bytesRead);
        written += bytesRead;
        lastDataTime = millis(); 

        progress = (written * 100) / contentLength;
        if (progress != lastProgress && progress % 10 == 0) { 
          display.clearDisplay();
          display.setCursor(0, 10);
          display.println(F("Updating Firmware..."));
          display.print(F("Progress: "));
          display.print(progress);
          display.println(F("%"));
          display.display();
          lastProgress = progress;
        }
      }
    }
    if (millis() - lastDataTime > timeoutDuration) {
      Update.abort();
      return false;
    }
    
    // CRITICAL FIX: delay(2) allows the background WiFi task to run better than yield() on single-core
    delay(2); 
  }
  return (written == contentLength) && Update.end();
}