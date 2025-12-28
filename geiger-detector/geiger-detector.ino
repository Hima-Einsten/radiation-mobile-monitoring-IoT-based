#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <WebServer.h> 

// ==========================================
// 1. KONFIGURASI WIFI & TELEGRAM
// ==========================================
const char* ssid      = "WIFI_SSID";     
const char* password  = "wifi_password";  
const char* botToken  = "your-bot-token"; 
const char* chatID    = "your_ID_Chat";          

// ==========================================
// 2. KONFIGURASI KALIBRASI & HARDWARE (PERUBAHAN DI SINI)
// ==========================================
#define GEIGER_PIN 4    
#define BUZZER_PIN 14   
LiquidCrystal_I2C lcd(0x27, 20, 4); 

// --- FAKTOR KALIBRASI BARU ---
// Berasal dari Regresi Linier: Dosis_Std = K * Dosis_Kit + Intersep
const float CALIBRATION_FACTOR_K = 0.3859; // Faktor Koreksi K
const float CALIBRATION_INTERCEPT = 0.2319; // Nilai intersep (background) µSv/jam

// Faktor konversi CPM ke uSv/jam bawaan detektor (151.0)
// Faktor ini masih digunakan untuk mendapatkan 'Dosis_Kit' awal.
const float INITIAL_CONV_FACTOR = 151.0; 


// ==========================================
// 3. VARIABEL & OBJEK SYSTEM
// ==========================================
WiFiClientSecure client;
UniversalTelegramBot bot(botToken, client);
WebServer server(80); 

volatile unsigned long pulseCount = 0;
unsigned long currentMillis = 0;
unsigned long previousMillis = 0;
const int LOG_PERIOD = 1000; 

// Variabel Data Global
unsigned long cps = 0;
unsigned long cpm = 0;
float uSv = 0.0;
String statusSystem = "AMAN";
String colorStatus = "#2ecc71"; // Hijau default

// Threshold
const int THRESHOLD_WASPADA = 2; 
const int THRESHOLD_BAHAYA  = 5; 
unsigned long lastTelegramTime = 0;
const int TELEGRAM_DELAY = 60000; 

// ==========================================
// 4. INTERRUPT
// ==========================================
void IRAM_ATTR onPulse() {
  pulseCount++;
}

// ==========================================
// 5. HTML & CSS MODERN (Tampilan Menyamping)
// ==========================================
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Radiation Monitor</title>
  <style>
    :root {
      --bg-color: #f4f7f6;
      --card-bg: #ffffff;
      --text-primary: #333333;
      --text-secondary: #7f8c8d;
    }
    body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background-color: var(--bg-color); margin: 0; text-align: center; }
    
    /* Header Dinamis */
    .header { padding: 15px; color: white; transition: background-color 0.5s ease; box-shadow: 0 2px 10px rgba(0,0,0,0.2); margin-bottom: 20px;}
    h1 { margin: 0; font-size: 1.2rem; text-transform: uppercase; letter-spacing: 2px; }
    #status-text { font-size: 2rem; font-weight: bold; margin-top: 5px; }

    /* Container Grid - MODIFIKASI AGAR MENYAMPING */
    .container { 
      display: flex; 
      flex-wrap: nowrap; /* KUNCI: Jangan biarkan turun ke bawah (stacking) */
      justify-content: space-between; /* Sebar merata */
      align-items: stretch; /* Tinggi kartu sama */
      padding: 0 10px; /* Padding kiri kanan */
      width: 95%; /* Gunakan hampir seluruh lebar layar */
      max-width: 1200px;
      margin: auto;
      gap: 10px; /* Jarak antar kartu */
      box-sizing: border-box;
    }
    
    /* Cards - MODIFIKASI UKURAN */
    .card { 
      background-color: var(--card-bg); 
      border-radius: 12px; 
      box-shadow: 0 4px 15px rgba(0,0,0,0.05); 
      padding: 15px 10px; 
      flex: 1; /* Semua kartu punya ukuran proporsional sama */
      min-width: 0; /* Biarkan kartu mengecil jika layar sempit */
      transition: transform 0.2s; 
      display: flex;
      flex-direction: column;
      justify-content: center;
      align-items: center;
    }
    .card:hover { transform: translateY(-5px); }
    
    .icon { font-size: 24px; margin-bottom: 5px; display: block; }
    
    /* Font Size Responsif (Agar muat di HP) */
    .value { font-size: 2rem; font-weight: 700; color: var(--text-primary); margin: 5px 0; word-wrap: break-word;}
    .unit { font-size: 0.8rem; color: var(--text-secondary); font-weight: 500; }
    .label { font-size: 0.75rem; color: var(--text-secondary); text-transform: uppercase; letter-spacing: 0.5px; font-weight: bold;}

    /* Progress Bar Radiasi */
    .meter-container { width: 80%; background-color: #e0e0e0; border-radius: 20px; height: 6px; margin-top: 8px; overflow: hidden; }
    #radiation-bar { height: 100%; width: 0%; background-color: #2ecc71; transition: width 0.5s, background-color 0.5s; }

    /* Footer */
    .footer { margin-top: 30px; color: #aaa; font-size: 0.8rem; padding-bottom: 20px; }

    /* Responsif Khusus HP Layar Kecil */
    @media (max-width: 600px) {
      .value { font-size: 1.5rem; }
      .icon { font-size: 20px; }
      .container { width: 100%; padding: 5px; gap: 5px; }
    }
  </style>
</head>
<body>

  <div class="header" id="header-bg" style="background-color: #2ecc71;">
    <h1>Status Area</h1>
    <div id="status-text">AMAN</div>
  </div>

  <div class="container">
    <div class="card">
      <span class="icon">⚡</span> <div class="label">Intensitas</div>
      <div class="value" id="cps">0</div>
      <div class="unit">CPS</div>
      <div class="meter-container">
        <div id="radiation-bar"></div>
      </div>
    </div>

    <div class="card">
      <span class="icon">📊</span> <div class="label">Rata-rata</div>
      <div class="value" id="cpm">0</div>
      <div class="unit">CPM</div>
    </div>

    <div class="card">
      <span class="icon">☢</span> <div class="label">Dosis</div>
      <div class="value" id="usv">0.000</div>
      <div class="unit">uSv/h</div>
    </div>
  </div>

  <div class="footer">
    Sistem Monitoring Radiasi PKM - Kelompok 3<br>
    ESP32 IoT Dashboard
  </div>

<script>
  // Fungsi untuk mengambil data dari ESP32 tanpa refresh halaman
  setInterval(function() {
    getData();
  }, 1000); // Update setiap 1000ms (1 detik)

  function getData() {
    var xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function() {
      if (this.readyState == 4 && this.status == 200) {
        // Parsing data JSON dari ESP32
        var data = JSON.parse(this.responseText);
        
        // Update Angka
        document.getElementById("cps").innerHTML = data.cps;
        document.getElementById("cpm").innerHTML = data.cpm;
        document.getElementById("usv").innerHTML = parseFloat(data.usv).toFixed(4); // 4 angka belakang koma
        
        // Update Status & Warna
        document.getElementById("status-text").innerHTML = data.status;
        document.getElementById("header-bg").style.backgroundColor = data.color;
        document.getElementById("radiation-bar").style.backgroundColor = data.color;

        // Update Lebar Bar (Visualisasi)
        var percentage = (data.cps / 10) * 100; 
        if(percentage > 100) percentage = 100;
        document.getElementById("radiation-bar").style.width = percentage + "%";
      }
    };
    xhttp.open("GET", "/data", true);
    xhttp.send();
  }
</script>
</body>
</html>
)rawliteral";

// ==========================================
// 6. FUNGSI HANDLER WEB SERVER
// ==========================================

// Mengirimkan Halaman Utama
void handleRoot() {
  server.send(200, "text/html", index_html);
}

// Mengirimkan Data JSON
void handleData() {
  String json = "{";
  json += "\"cps\":" + String(cps) + ",";
  json += "\"cpm\":" + String(cpm) + ",";
  json += "\"usv\":" + String(uSv, 4) + ",";
  json += "\"status\":\"" + statusSystem + "\",";
  json += "\"color\":\"" + colorStatus + "\"";
  json += "}";
  
  server.send(200, "application/json", json);
}

// ==========================================
// 7. SETUP
// ==========================================
void setup() {
  Serial.begin(115200);
  pinMode(GEIGER_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  attachInterrupt(digitalPinToInterrupt(GEIGER_PIN), onPulse, FALLING);
  
  Wire.begin(21, 22);
  lcd.init(); lcd.backlight();

  // Koneksi WiFi
  lcd.setCursor(0, 0); lcd.print("Connecting WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }

  if(WiFi.status() == WL_CONNECTED) {
    client.setInsecure();
    
    server.on("/", handleRoot);    
    server.on("/data", handleData);  
    
    server.begin();

    // Info di LCD
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("WiFi Connected!");
    lcd.setCursor(0, 1); lcd.print("IP: "); 
    lcd.print(WiFi.localIP());
    
    bot.sendMessage(chatID, "🟢 Sistem ONLINE. Web: http://" + WiFi.localIP().toString(), "");
  }
  
  delay(3000); 
  lcd.clear();
}

// ==========================================
// 8. LOOP (PERUBAHAN DI SINI)
// ==========================================
void loop() {
  server.handleClient(); // Handle permintaan web
  
  currentMillis = millis();
  if (currentMillis - previousMillis >= LOG_PERIOD) {
    previousMillis = currentMillis;

    unsigned long counts = pulseCount;
    pulseCount = 0;
    
    cps = counts;
    cpm = cps * 60;
    
    // --- IMPLEMENTASI FAKTOR KALIBRASI ---
    // 1. Hitung laju dosis "mentah" detektor (Dosis_Kit)
    float uSv_kit = cpm / INITIAL_CONV_FACTOR;

    // 2. Koreksi dengan Regresi Linier
    // uSv (Koreksi) = K * uSv_kit + Intersep
    uSv = (CALIBRATION_FACTOR_K * uSv_kit) + CALIBRATION_INTERCEPT;
    
    // Pastikan uSv tidak negatif
    if (uSv < 0.0) {
      uSv = 0.0;
    }

    // --- LOGIKA STATUS ---
    // Logika Status tetap menggunakan CPS (Counts Per Second) untuk respons cepat
    if (cps >= THRESHOLD_BAHAYA) {
      statusSystem = "BAHAYA!";
      colorStatus = "#e74c3c"; // Merah
      digitalWrite(BUZZER_PIN, HIGH);
      lcd.setCursor(0, 0); lcd.print("Status: BAHAYA!!  ");
      
      if (millis() - lastTelegramTime > TELEGRAM_DELAY) {
        if (WiFi.status() == WL_CONNECTED) {
            // Gunakan uSv yang sudah dikalibrasi dalam pesan Telegram
            String msg = "⛔ BAHAYA RADIASI!\nCPS: " + String(cps) + "\nDosis: " + String(uSv, 4) + " uSv/jam\nLink: http://" + WiFi.localIP().toString();
            bot.sendMessage(chatID, msg, "");
        }
        lastTelegramTime = millis();
      }
    } 
    else if (cps >= THRESHOLD_WASPADA) {
      statusSystem = "WASPADA";
      colorStatus = "#f39c12"; // Oranye
      digitalWrite(BUZZER_PIN, LOW);
      lcd.setCursor(0, 0); lcd.print("Status: WASPADA   ");
    } 
    else {
      statusSystem = "AMAN";
      colorStatus = "#2ecc71"; // Hijau
      digitalWrite(BUZZER_PIN, LOW);
      lcd.setCursor(0, 0); lcd.print("Status: AMAN      ");
    }

    // LCD Tetap Update
    lcd.setCursor(0, 1); lcd.print("CPS : "); lcd.print(cps); lcd.print("    ");
    lcd.setCursor(0, 2); lcd.print("uSv : "); lcd.print(uSv, 4); // Tampilkan 4 desimal
    lcd.setCursor(0, 3); lcd.print("IP: "); lcd.print(WiFi.localIP()); 
  }
}