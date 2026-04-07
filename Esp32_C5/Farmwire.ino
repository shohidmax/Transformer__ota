/*
  Transformer Security Node - ESP32-C5 (with GitHub OTA & WiFiManager)
  ------------------------------------
  Hardware: 
  - ESP32-C5 N8R4
  - 1x PIR Sensor
  - 2x HLK-LD2410C mmWave Sensors
  - 1x I2C OLED Display (SSD1306 128x64)

  Wiring:
  - PIR OUT       -> GPIO 10
  - RADAR 1 OUT   -> GPIO 4
  - RADAR 2 OUT   -> GPIO 5
  - ALARM RELAY   -> GPIO 1
  - BUZZER        -> GPIO 3
  - OLED SDA      -> GPIO 8
  - OLED SCL      -> GPIO 9
*/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h> // Include WiFiManager Library

// --- Pin Definitions ---
#define PIR_PIN 10
#define RADAR_1_PIN 4
#define RADAR_2_PIN 5
#define ALARM_RELAY_PIN 2
#define BUZZER_PIN 1 

#define I2C_SDA 8
#define I2C_SCL 9

// --- OLED Configuration ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --- OTA Configuration ---
const char* firmwareUrl = "https://github.com/shohidmax/Transformer__ota/releases/download/transformerq/Transfer_Sec.ino.bin";
const char* versionUrl = "https://raw.githubusercontent.com/shohidmax/Transformer__ota/refs/heads/main/Esp32_C5/virsion.txt";

const char* currentFirmwareVersion = "1.0.0";
const unsigned long updateCheckInterval = 5 * 60 * 1000; // 5 minutes
unsigned long lastUpdateCheck = 0;

// --- State Variables ---
bool pirMotion = false;
bool radar1Presence = false;
bool radar2Presence = false;

// Security States: 0 = Safe, 1 = Warning (PIR only), 2 = ALARM (Radar Confirmed)
int securityState = 0; 

// --- Function Prototypes ---
void connectToWiFi();
void checkForFirmwareUpdate();
String fetchLatestVersion();
void downloadAndApplyFirmware();
bool startOTAUpdate(WiFiClient* client, int contentLength);

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // Initialize Pins
  pinMode(PIR_PIN, INPUT);
  pinMode(RADAR_1_PIN, INPUT);
  pinMode(RADAR_2_PIN, INPUT);
  
  pinMode(ALARM_RELAY_PIN, OUTPUT);
  digitalWrite(ALARM_RELAY_PIN, LOW); // Siren off by default
  
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW); // Buzzer off by default

  // Initialize I2C for OLED
  Wire.begin(I2C_SDA, I2C_SCL);

  // Initialize OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Loop forever if display fails
  }
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 20);
  display.println(F("SYSTEM BOOTING..."));
  display.display();
  
  // Connect to WiFi (using WiFiManager) and Check for OTA
  connectToWiFi();
  
  if (WiFi.status() == WL_CONNECTED) {
    display.clearDisplay();
    display.setCursor(0, 10);
    display.println(F("WiFi OK. Waiting..."));
    display.display();
    delay(3000); // Give router 3 seconds to finalize routing

    checkForFirmwareUpdate();
  }
  
  // Allow sensors to stabilize
  display.clearDisplay();
  display.setCursor(10, 20);
  display.println(F("Stabilizing Sensors"));
  display.display();
  delay(5000); 
}

void loop() {
  // 1. Non-blocking Timer to check for updates
  if (millis() - lastUpdateCheck >= updateCheckInterval) {
    lastUpdateCheck = millis();
    
    if (WiFi.status() == WL_CONNECTED) {
      checkForFirmwareUpdate();
    } else {
      Serial.println("WiFi lost, attempting auto-reconnect...");
      WiFi.reconnect(); // Attempt a non-blocking background reconnect
    }
  }

  // 2. Read all sensors
  pirMotion = digitalRead(PIR_PIN);
  radar1Presence = digitalRead(RADAR_1_PIN);
  radar2Presence = digitalRead(RADAR_2_PIN);

  // 3. Security Logic Evaluation
  if (radar1Presence || radar2Presence) {
    securityState = 2;
    digitalWrite(ALARM_RELAY_PIN, HIGH); // TURN ON SIREN!
  } else if (pirMotion) {
    securityState = 1;
    digitalWrite(ALARM_RELAY_PIN, LOW); // Keep siren off
  } else {
    securityState = 0;
    digitalWrite(ALARM_RELAY_PIN, LOW); // Siren off
  }

  // 4. Update OLED Dashboard
  updateDisplay();

  // 5. Handle Buzzer Sounds
  handleBuzzer();

  // Small delay for loop stability
  delay(100);
}

void handleBuzzer() {
  if (securityState == 0) {
    digitalWrite(BUZZER_PIN, LOW);
  } 
  else if (securityState == 1) {
    if (millis() % 1000 < 500) {
      digitalWrite(BUZZER_PIN, HIGH);
    } else {
      digitalWrite(BUZZER_PIN, LOW);
    }
  } 
  else if (securityState == 2) {
    if (millis() % 200 < 100) {
      digitalWrite(BUZZER_PIN, HIGH);
    } else {
      digitalWrite(BUZZER_PIN, LOW);
    }
  }
}

void updateDisplay() {
  display.clearDisplay();

  // Draw Header
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(F("TRANSFORMER SECURITY"));
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

  // Draw Sensor Status
  display.setCursor(0, 15);
  display.print(F("PIR (Long)  : "));
  display.println(pirMotion ? F("DETECTED") : F("CLEAR"));
  
  display.print(F("RDR 1 (Left): "));
  display.println(radar1Presence ? F("TARGET!") : F("CLEAR"));

  display.print(F("RDR 2 (Rght): "));
  display.println(radar2Presence ? F("TARGET!") : F("CLEAR"));

  // Draw System Status (Large Text)
  display.drawLine(0, 42, 128, 42, SSD1306_WHITE);
  display.setCursor(0, 48);
  
  if (securityState == 0) {
    display.setTextSize(2);
    display.print(F("  SAFE  "));
  } else if (securityState == 1) {
    display.setTextSize(2);
    display.print(F("WARNING ")); 
  } else if (securityState == 2) {
    display.setTextSize(2);
    if (millis() % 500 < 250) {
      display.setTextColor(SSD1306_BLACK, SSD1306_WHITE); // Invert
    }
    display.print(F(" ALARM! "));
    display.setTextColor(SSD1306_WHITE, SSD1306_BLACK); // Reset
  }

  // Draw WiFi Status Icon
  display.setTextSize(1);
  display.setCursor(105, 55);
  if(WiFi.status() == WL_CONNECTED) {
    display.print(F("Wi-Fi"));
  } else {
    display.print(F("No-AP"));
  }

  display.display();
}

// ==========================================
//             OTA & WIFI FUNCTIONS
// ==========================================

void connectToWiFi() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 10);
  display.println(F("Connecting to WiFi.."));
  display.println(F("Or connect to AP:"));
  display.println(F("'Transformer_AP'"));
  display.display();

  Serial.println("Starting WiFiManager...");
  
  WiFiManager wm;
  
  // Set a timeout so the ESP doesn't get stuck forever if WiFi goes down
  // After 3 minutes (180 seconds) it will exit setup and continue running as an offline alarm
  wm.setConfigPortalTimeout(180);
  wm.setConnectTimeout(20); // Force it to timeout if the router connection hangs

  // This will try to connect to the last known network.
  // If it fails, it sets up an Access Point named "Transformer_AP" with password "admin123"
  bool res = wm.autoConnect("Transformer_AP", "admin123");
  
  display.clearDisplay();
  display.setCursor(0, 10);
  if (!res) {
    Serial.println("Failed to connect or hit timeout. Running Offline.");
    display.println(F("WiFi Timeout!"));
    display.println(F("Running Offline."));
  } else {
    Serial.println("\nWiFi connected");
    Serial.println("IP address: " + WiFi.localIP().toString());
    display.println(F("WiFi Connected!"));
    display.print(WiFi.localIP());
  }
  display.display();
  delay(2000);
}

void checkForFirmwareUpdate() {
  Serial.println("Checking for firmware update...");

  String latestVersion = fetchLatestVersion();
  
  if (latestVersion == "") {
    Serial.println("Could not verify latest version.");
    return;
  }

  Serial.println("Current Version: " + String(currentFirmwareVersion));
  Serial.println("Latest Version: " + latestVersion);

  if (latestVersion != currentFirmwareVersion) {
    Serial.println("Update available. Starting OTA...");
    
    // Show update status on OLED
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 10);
    display.println(F("NEW UPDATE FOUND!"));
    display.println(F("Downloading..."));
    display.display();
    
    downloadAndApplyFirmware();
  } else {
    Serial.println("Device is up to date.");
  }
}

String fetchLatestVersion() {
  WiFiClientSecure client;
  client.setInsecure(); // Skip certificate validation for GitHub
  client.setTimeout(15000); // Increase socket timeout to 15s to allow slow handshakes
  
  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(15000); // 15 seconds timeout to prevent premature drop
  
  if (!http.begin(client, versionUrl)) {
    Serial.println("Unable to connect to Version URL");
    return "";
  }

  int httpCode = http.GET();
  String latestVersion = "";

  if (httpCode == HTTP_CODE_OK) {
    latestVersion = http.getString();
    latestVersion.trim(); // Removes \n or \r from file
  } else {
    Serial.printf("Failed to fetch version. HTTP code: %d\n", httpCode);
  }
  
  http.end();
  return latestVersion;
}

void downloadAndApplyFirmware() {
  WiFiClientSecure client;
  client.setInsecure(); 
  client.setTimeout(15000); // Increase socket timeout to 15s

  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(15000); // 15 seconds timeout 
  
  if (!http.begin(client, firmwareUrl)) {
     Serial.println("Unable to connect to Firmware URL");
     return;
  }

  int httpCode = http.GET();
  Serial.printf("HTTP GET code: %d\n", httpCode);

  if (httpCode == HTTP_CODE_OK) {
    int contentLength = http.getSize();
    Serial.printf("Firmware size: %d bytes\n", contentLength);

    if (contentLength > 0) {
      WiFiClient* stream = http.getStreamPtr();
      if (startOTAUpdate(stream, contentLength)) {
        Serial.println("OTA Success! Restarting...");
        
        display.clearDisplay();
        display.setCursor(0, 20);
        display.println(F("UPDATE SUCCESS!"));
        display.println(F("Rebooting..."));
        display.display();
        
        delay(1000);
        ESP.restart();
      } else {
        Serial.println("OTA Failed.");
      }
    } else {
      Serial.println("Invalid content length (0 or -1).");
    }
  } else {
    Serial.printf("Firmware download failed. HTTP code: %d\n", httpCode);
  }
  http.end();
}

bool startOTAUpdate(WiFiClient* client, int contentLength) {
  if (!Update.begin(contentLength)) {
    Serial.printf("Not enough space to begin OTA. Error: %s\n", Update.errorString());
    return false;
  }

  Serial.println("Writing firmware to flash...");
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
        if (progress != lastProgress && progress % 10 == 0) { // Update display every 10%
          Serial.printf("Progress: %d%%\n", progress);
          
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
      Serial.println("\nError: Connection timed out.");
      Update.abort();
      return false;
    }
    yield(); // Prevent Watchdog reset
  }

  if (written != contentLength) {
    Serial.printf("\nError: Written %d / %d bytes. Download incomplete.\n", written, contentLength);
    Update.abort();
    return false;
  }

  if (!Update.end()) {
    Serial.printf("\nError: %s\n", Update.errorString());
    return false;
  }

  return true;
}