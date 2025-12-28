#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

// ================= PIN L298N =================
#define IN1 4
#define IN2 5
#define IN3 6
#define IN4 7

// ================= SERVO =================
#define SERVO_PIN 13
Servo scanServo;

// ================= WIFI =================
const char* ssid = "SSID_WIFI";
const char* password = "wifi_password";

WebServer server(80);

// ================= MOTOR =================
void stopMotor() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

void maju() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void mundur() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

void kiri() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void kanan() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

// ================= SERVO SCAN (~10 DETIK) =================
void servo_scan() {
  // dari tengah ke kanan
  for (int pos = 90; pos <= 180; pos++) {
    scanServo.write(pos);
    delay(55);
  }

  // dari kanan ke kiri
  for (int pos = 180; pos >= 0; pos--) {
    scanServo.write(pos);
    delay(55);
  }

  // kembali ke tengah
  scanServo.write(90);
}

// ================= WEB =================
String webpage() {
  return R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
button {
  width: 130px;
  height: 60px;
  font-size: 18px;
  margin: 6px;
}
.stop {
  background: red;
  color: white;
}
.scan {
  background: orange;
}
</style>
</head>

<body align="center">
<h2>ESP32 CAR CONTROL</h2>

<button onmousedown="fetch('/maju')" onmouseup="fetch('/stop')" ontouchstart="fetch('/maju')" ontouchend="fetch('/stop')">MAJU</button><br>

<button onmousedown="fetch('/kiri')" onmouseup="fetch('/stop')" ontouchstart="fetch('/kiri')" ontouchend="fetch('/stop')">KIRI</button>

<button onmousedown="fetch('/kanan')" onmouseup="fetch('/stop')" ontouchstart="fetch('/kanan')" ontouchend="fetch('/stop')">KANAN</button><br>

<button onmousedown="fetch('/mundur')" onmouseup="fetch('/stop')" ontouchstart="fetch('/mundur')" ontouchend="fetch('/stop')">MUNDUR</button><br>

<button class="stop" onclick="fetch('/stop')">STOP</button><br><br>

<button class="scan" onclick="fetch('/scan')">SCAN</button>

</body>
</html>
)rawliteral";
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  stopMotor();

  scanServo.attach(SERVO_PIN);
  scanServo.write(90);

  WiFi.begin(ssid, password);
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nConnected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  server.on("/", []() {
    server.send(200, "text/html", webpage());
  });

  server.on("/maju", []() {
    maju();
    server.send(200, "text/plain", "MAJU");
  });

  server.on("/mundur", []() {
    mundur();
    server.send(200, "text/plain", "MUNDUR");
  });

  server.on("/kiri", []() {
    kiri();
    server.send(200, "text/plain", "KIRI");
  });

  server.on("/kanan", []() {
    kanan();
    server.send(200, "text/plain", "KANAN");
  });

  server.on("/stop", []() {
    stopMotor();
    server.send(200, "text/plain", "STOP");
  });

  server.on("/scan", []() {
    servo_scan();
    server.send(200, "text/plain", "SCAN");
  });

  server.begin();
}

// ================= LOOP =================
void loop() {
  server.handleClient();
}