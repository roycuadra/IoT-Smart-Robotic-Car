#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>

// ================== PIN MAP ==================
const uint8_t IN1 = D1;
const uint8_t IN2 = D2;
const uint8_t IN3 = D3;
const uint8_t IN4 = D4;

const uint8_t LED_PIN = D5;      // indicator LED (external)
const uint8_t TRIG_PIN = D6;     // HC-SR04 Trigger
const uint8_t ECHO_PIN = D7;     // HC-SR04 Echo

// ================== Wi-Fi Config ==================
const char *ssid = "RobotCar_AP";
const char *password = "12345678";

// ================== Server Setup ==================
DNSServer dnsServer;
ESP8266WebServer server(80);
WebSocketsServer webSocket(81);

// ================== Parameters ==================
const int COLLISION_DISTANCE_CM = 20;      // obstacle threshold (blocks forward)
const unsigned long SENSOR_INTERVAL_MS = 120; // ultrasonic sampling
const unsigned long LED_BLINK_PERIOD_MS = 400;

// ================== State ==================
unsigned long lastSensorMillis = 0;
unsigned long ledBlinkMillis = 0;
bool ledBlinkState = false;

bool collisionDetected = false; // sensor-based
bool ledManualOn = false;       // toggled by LED_TOGGLE and persists across operation
float currentDistance = -1.0f;  // latest measured distance

// ================== HTML PAGE ==================
const char htmlPage[] PROGMEM = R"rawliteral(
<!doctype html>
<html><head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta charset="utf-8"><title>Robot Control</title>
<style>
:root{
  --bg-primary: #0a0e1a;
  --bg-secondary: #111827;
  --bg-card: #1a1f35;
  --accent-blue: #3b82f6;
  --accent-blue-hover: #2563eb;
  --accent-green: #10b981;
  --text-primary: #f9fafb;
  --text-secondary: #9ca3af;
  --border: rgba(255,255,255,0.08);
  --danger: #ef4444;
  --shadow: rgba(0,0,0,0.3);
}

* {
  margin: 0;
  padding: 0;
  box-sizing: border-box;
  -webkit-user-select: none;
  -moz-user-select: none;
  user-select: none;
}

html, body {
  height: 100%;
  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', 'Inter', sans-serif;
  background: linear-gradient(135deg, var(--bg-primary) 0%, var(--bg-secondary) 100%);
  color: var(--text-primary);
  overflow-x: hidden;
}

.container {
  min-height: 100vh;
  display: flex;
  align-items: center;
  justify-content: center;
  padding: 20px;
}

.card {
  width: 100%;
  max-width: 420px;
  background: var(--bg-card);
  border-radius: 24px;
  padding: 32px 24px;
  border: 1px solid var(--border);
  box-shadow: 0 20px 60px var(--shadow);
  backdrop-filter: blur(10px);
}

.header {
  text-align: center;
  margin-bottom: 32px;
  padding-bottom: 24px;
  border-bottom: 1px solid var(--border);
}

.title {
  font-size: 28px;
  font-weight: 700;
  letter-spacing: -0.5px;
  margin-bottom: 8px;
  background: linear-gradient(135deg, #60a5fa 0%, #3b82f6 100%);
  -webkit-background-clip: text;
  -webkit-text-fill-color: transparent;
  background-clip: text;
}

.subtitle {
  font-size: 13px;
  color: var(--text-secondary);
  line-height: 1.6;
}

.controls {
  display: flex;
  flex-direction: column;
  gap: 16px;
}

.control-row {
  display: flex;
  justify-content: space-between;
  align-items: center;
  gap: 12px;
}

.control-center {
  display: flex;
  justify-content: center;
}

.btn {
  padding: 16px 32px;
  border-radius: 14px;
  background: linear-gradient(135deg, var(--accent-blue) 0%, var(--accent-blue-hover) 100%);
  color: white;
  font-weight: 600;
  font-size: 15px;
  border: none;
  cursor: pointer;
  transition: all 0.2s ease;
  box-shadow: 0 4px 16px rgba(59, 130, 246, 0.3);
  position: relative;
  overflow: hidden;
}

.btn:hover {
  transform: translateY(-2px);
  box-shadow: 0 6px 24px rgba(59, 130, 246, 0.4);
}

.btn:active {
  transform: translateY(0);
}

.btn:disabled {
  opacity: 0.5;
  cursor: not-allowed;
  transform: none !important;
}

.btn-small {
  padding: 12px 24px;
  font-size: 14px;
  flex: 1;
}

.btn-ghost {
  background: rgba(255, 255, 255, 0.05);
  border: 1px solid var(--border);
  box-shadow: none;
}

.btn-ghost:hover {
  background: rgba(255, 255, 255, 0.08);
  box-shadow: none;
}

.led-section {
  display: flex;
  align-items: center;
  gap: 12px;
  padding: 12px 16px;
  background: rgba(255, 255, 255, 0.03);
  border-radius: 12px;
  border: 1px solid var(--border);
}

.led {
  width: 12px;
  height: 12px;
  border-radius: 50%;
  background: rgba(255, 255, 255, 0.1);
  transition: all 0.3s ease;
  box-shadow: inset 0 2px 4px rgba(0, 0, 0, 0.3);
}

.led.on {
  background: var(--accent-green);
  box-shadow: 0 0 20px rgba(16, 185, 129, 0.6), 0 0 40px rgba(16, 185, 129, 0.3);
}

.btn-led {
  flex: 1;
  padding: 8px 16px;
  font-size: 13px;
  border-radius: 8px;
}

.connection-status {
  font-size: 12px;
  color: var(--text-secondary);
  padding: 8px 12px;
  background: rgba(255, 255, 255, 0.03);
  border-radius: 8px;
  border: 1px solid var(--border);
  text-align: center;
  font-weight: 500;
}

.status-panel {
  margin-top: 24px;
  padding: 20px;
  background: rgba(255, 255, 255, 0.03);
  border-radius: 16px;
  border: 1px solid var(--border);
}

.status-row {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 12px 0;
}

.status-row:not(:last-child) {
  border-bottom: 1px solid var(--border);
}

.status-label {
  font-size: 14px;
  color: var(--text-secondary);
  font-weight: 500;
}

.status-value {
  font-size: 18px;
  font-weight: 700;
  color: var(--text-primary);
}

.warning {
  color: var(--danger);
  font-weight: 700;
  font-size: 14px;
  animation: pulse 1.5s ease-in-out infinite;
}

@keyframes pulse {
  0%, 100% { opacity: 1; }
  50% { opacity: 0.6; }
}

.footer {
  margin-top: 24px;
  padding-top: 20px;
  border-top: 1px solid var(--border);
  text-align: center;
  font-size: 12px;
  color: var(--text-secondary);
  line-height: 1.6;
}

@media (max-width: 480px) {
  .card {
    padding: 24px 20px;
  }
  
  .title {
    font-size: 24px;
  }
  
  .btn {
    padding: 14px 28px;
    font-size: 14px;
  }
  
  .btn-small {
    padding: 10px 20px;
    font-size: 13px;
  }
}
</style>
</head><body>
<div class="container">
  <div class="card">
    <div class="header">
      <div class="title">Robot Control</div>
      <div class="subtitle">HC-SR04 Collision Detection<br>LED Force Blink on Obstacle</div>
    </div>

    <div class="controls">
      <div class="control-center">
        <button id="btnF" class="btn" data-cmd="F">FORWARD</button>
      </div>

      <div class="control-row">
        <button id="btnL" class="btn btn-small" data-cmd="L">LEFT</button>
        <button id="btnS" class="btn btn-ghost btn-small" data-cmd="S">STOP</button>
        <button id="btnR" class="btn btn-small" data-cmd="R">RIGHT</button>
      </div>

      <div class="control-center">
        <button id="btnB" class="btn" data-cmd="B">BACK</button>
      </div>

      <div class="control-row" style="margin-top: 8px;">
        <div class="led-section">
          <span id="ledIndicator" class="led"></span>
          <button id="btnToggleLED" class="btn btn-ghost btn-led">Toggle LED</button>
        </div>
        <div id="connText" class="connection-status">Connecting...</div>
      </div>
    </div>

    <div class="status-panel">
      <div class="status-row">
        <span class="status-label">Distance</span>
        <span id="distanceText" class="status-value">-- cm</span>
      </div>
      <div class="status-row">
        <span class="status-label">Status</span>
        <span id="collisionText" class="status-value">—</span>
      </div>
    </div>

    <div class="footer">
      Hold buttons for continuous movement<br>
      Forward disabled during obstacle detection<br>
      <br>
      <strong>Created by Roy Cuadra @2025</strong>
    </div>
  </div>
</div>

<script>
(function(){
  let ws;
  const connText=document.getElementById('connText');
  const distanceText=document.getElementById('distanceText');
  const collisionText=document.getElementById('collisionText');
  const ledIndicator=document.getElementById('ledIndicator');

  const btnF=document.getElementById('btnF');
  const btnB=document.getElementById('btnB');
  const btnL=document.getElementById('btnL');
  const btnR=document.getElementById('btnR');
  const btnS=document.getElementById('btnS');
  const btnToggleLED=document.getElementById('btnToggleLED');

  function connect(){
    ws = new WebSocket('ws://'+location.hostname+':81/');
    ws.onopen = ()=> connText.textContent='Connected';
    ws.onclose = ()=> connText.textContent='Disconnected';
    ws.onerror = ()=> connText.textContent='WS Error';
    ws.onmessage = (e)=> handleMsg(e.data);
  }
  function handleMsg(msg){
    if(msg.startsWith('DIST:')){
      distanceText.textContent = msg.split(':')[1] + ' cm';
    } else if(msg==='COLLIDE'){
      collisionText.innerHTML = '<span class="warning">OBSTACLE</span>';
    } else if(msg==='BLOCK_FWD'){
      btnF.disabled=true; btnF.classList.add('btn-ghost');
      collisionText.innerHTML = '<span class="warning">FORWARD BLOCKED</span>';
    } else if(msg==='CLEAR_BLOCK'){
      btnF.disabled=false; btnF.classList.remove('btn-ghost');
      collisionText.textContent = '—';
    } else if(msg==='LED_ON'){
      ledIndicator.classList.add('on');
    } else if(msg==='LED_OFF'){
      ledIndicator.classList.remove('on');
    }
  }

  function send(cmd){
    if(ws && ws.readyState===WebSocket.OPEN) ws.send(cmd);
  }

  btnF.addEventListener('pointerdown', ()=> send('F') );
  btnF.addEventListener('pointerup', ()=> send('S') );
  btnB.addEventListener('pointerdown', ()=> send('B') );
  btnB.addEventListener('pointerup', ()=> send('S') );
  btnL.addEventListener('pointerdown', ()=> send('L') );
  btnL.addEventListener('pointerup', ()=> send('S') );
  btnR.addEventListener('pointerdown', ()=> send('R') );
  btnR.addEventListener('pointerup', ()=> send('S') );
  btnS.addEventListener('click', ()=> send('S') );
  btnToggleLED.addEventListener('click', ()=> send('LED_TOGGLE') );

  connect();
  setInterval(()=> {
    if(!ws) { connText.textContent='Not connected'; return; }
    if(ws.readyState===WebSocket.OPEN) connText.textContent='Connected';
    else if(ws.readyState===WebSocket.CONNECTING) connText.textContent='Connecting...';
    else connText.textContent='Disconnected';
  },700);
})();
</script>
</body></html>
)rawliteral";

// ================== Movement helpers ==================
void forwardMove(){ 
  if(collisionDetected){ stopMotor(); return; }
  digitalWrite(IN1, HIGH); 
  digitalWrite(IN2, LOW); 
  digitalWrite(IN3, HIGH); 
  digitalWrite(IN4, LOW); 
}

void backwardMove(){
  digitalWrite(IN1, LOW); 
  digitalWrite(IN2, HIGH); 
  digitalWrite(IN3, LOW); 
  digitalWrite(IN4, HIGH); 
}

void leftMove(){
  digitalWrite(IN1, LOW); 
  digitalWrite(IN2, HIGH); 
  digitalWrite(IN3, HIGH); 
  digitalWrite(IN4, LOW); 
}

void rightMove(){
  digitalWrite(IN1, HIGH); 
  digitalWrite(IN2, LOW); 
  digitalWrite(IN3, LOW); 
  digitalWrite(IN4, HIGH); 
}

void stopMotor(){
  digitalWrite(IN1, LOW); 
  digitalWrite(IN2, LOW); 
  digitalWrite(IN3, LOW); 
  digitalWrite(IN4, LOW); 
}

// ================== WebSocket helpers ==================
void broadcastStatus(const char *msg) {
  String payload = msg;
  webSocket.broadcastTXT(payload);
}

// ================== Ultrasonic ==================
float readUltrasonicCM(){
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  unsigned long duration = pulseIn(ECHO_PIN, HIGH, 30000UL);
  if(duration == 0) return -1.0f;
  return (float)duration / 58.2f;
}

// ================== WebSocket message handler ==================
void handleWsMessage(uint8_t num, WStype_t type, uint8_t * payload, size_t length){
  if(type != WStype_TEXT) return;
  char buf[128];
  if(length > 127) length = 127;
  memcpy(buf, payload, length);
  buf[length] = '\0';
  String cmd = String(buf);
  cmd.trim();

  if(cmd == "F"){ forwardMove(); return; }
  if(cmd == "B"){ backwardMove(); return; }
  if(cmd == "L"){ leftMove(); return; }
  if(cmd == "R"){ rightMove(); return; }
  if(cmd == "S"){ stopMotor(); return; }

  if(cmd == "LED_TOGGLE"){
    ledManualOn = !ledManualOn;
    digitalWrite(LED_PIN, ledManualOn ? HIGH : LOW);
    broadcastStatus(ledManualOn ? "LED_ON" : "LED_OFF");
  }
}

void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length){
  handleWsMessage(num, type, payload, length);
}

// ================== Setup ==================
void setup(){
  Serial.begin(115200);
  Serial.println("\nRobotCar (Manual only, forced LED blink) booting...");

  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  stopMotor();

  pinMode(LED_PIN, OUTPUT); digitalWrite(LED_PIN, LOW);
  pinMode(TRIG_PIN, OUTPUT); digitalWrite(TRIG_PIN, LOW);
  pinMode(ECHO_PIN, INPUT);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);
  IPAddress ip = WiFi.softAPIP();
  Serial.printf("AP started. IP=%s\n", ip.toString().c_str());

  dnsServer.start(53, "*", ip);
  server.onNotFound([](){ server.send_P(200, "text/html", htmlPage); });
  server.begin();
  webSocket.begin();
  webSocket.onEvent(onWebSocketEvent);

  lastSensorMillis = millis();
}

// ================== Loop ==================
void loop(){
  dnsServer.processNextRequest();
  server.handleClient();
  webSocket.loop();

  unsigned long now = millis();

  if(now - lastSensorMillis >= SENSOR_INTERVAL_MS){
    lastSensorMillis = now;
    float d = readUltrasonicCM();
    if(d > 0.0f){
      currentDistance = d;
      int dd = (int)round(d);
      char b[32]; snprintf(b, sizeof(b), "DIST:%d", dd);
      broadcastStatus(b);

      if(dd <= COLLISION_DISTANCE_CM){
        if(!collisionDetected){
          collisionDetected = true;
          stopMotor();
          broadcastStatus("COLLIDE");
          Serial.println("Collision detected - stopping");
        }
        if(now - ledBlinkMillis >= LED_BLINK_PERIOD_MS){
          ledBlinkMillis = now;
          ledBlinkState = !ledBlinkState;
          digitalWrite(LED_PIN, ledBlinkState ? HIGH : LOW);
          broadcastStatus(ledBlinkState ? "LED_ON" : "LED_OFF");
        }
        broadcastStatus("BLOCK_FWD");
      } else {
        if(collisionDetected){
          collisionDetected = false;
          digitalWrite(LED_PIN, ledManualOn ? HIGH : LOW);
          broadcastStatus(ledManualOn ? "LED_ON" : "LED_OFF");
          broadcastStatus("CLEAR_BLOCK");
          Serial.println("Collision cleared");
        }
        ledBlinkState = false;
      }
    }
  }
  yield();
}
