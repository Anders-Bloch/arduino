/**
 * ESP32 Robot Mower Controller
 * ============================
 * Differential-drive robot mower using two 31zy 12 V DC motors:
 *   - One L298N dual H-bridge module → speed (PWM) + direction (IN1–IN4)
 *
 * Access Point mode – no router needed.
 * 1. Connect your phone to the "MowerControl" WiFi network.
 * 2. Open http://192.168.4.1 in the browser.
 * 3. Hold a direction button to drive; release to stop.
 *    A 2-second watchdog cuts the motors if the connection drops.
 *
 * Wiring guide (L298N module)
 * ──────────────────────────────────────────────────────────────
 *  L298N power
 *    12 V Battery+  → L298N VCC (motor power)
 *    Battery− (GND) → L298N GND
 *    Remove ENA and ENB jumpers – connect to ESP32 instead.
 *
 *  LEFT motor (Motor A on L298N)
 *    ESP32 GPIO 25  → L298N ENA  (PWM speed)
 *    ESP32 GPIO 26  → L298N IN1  (direction)
 *    ESP32 GPIO 27  → L298N IN2  (direction)
 *    L298N OUT1 + OUT2  → Left motor terminals
 *
 *  RIGHT motor (Motor B on L298N)
 *    ESP32 GPIO 32  → L298N ENB  (PWM speed)
 *    ESP32 GPIO 33  → L298N IN3  (direction)
 *    ESP32 GPIO 14  → L298N IN4  (direction)
 *    L298N OUT3 + OUT4  → Right motor terminals
 *
 *  ESP32 GND  → L298N GND  (common ground)
 *  NOTE: L298N logic runs on 5 V – 3.3 V ESP32 signals are usually sufficient.
 *  If a motor runs the wrong way, swap its two L298N OUT wires.
 *
 *  CUTTER motor (single-channel relay module + 6 V supply)
 *    ESP32 GPIO 13  → Relay IN  (HIGH = relay on = motor runs)
 *    Relay VCC      → ESP32 3.3 V or 5 V (check module label)
 *    Relay GND      → ESP32 GND
 *    Relay COM      → 6 V Battery+
 *    Relay NO       → Cutter motor +
 *    Cutter motor − → 6 V Battery−
 *    NOTE: if your relay module is active-LOW, set CUTTER_RELAY_ON = LOW below.
 * Compatible with ESP32 Arduino Core v3.x
 * Board in Arduino IDE: "ESP32 Dev Module"
 * ──────────────────────────────────────────────────────────────
 */

#include <WiFi.h>
#include <WebServer.h>

// ── WiFi Access Point ────────────────────────────────────────
const char* AP_SSID     = "MowerControl";
const char* AP_PASSWORD = "mower1234";   // ≥8 chars; use "" for open network

// ── Pin assignments  (adjust to match your wiring) ───────────────
//  LEFT motor  – L298N Motor A
const int M1_ENA = 25;   // ENA  – PWM speed
const int M1_IN1 = 26;   // IN1  – direction
const int M1_IN2 = 27;   // IN2  – direction

//  RIGHT motor – L298N Motor B
const int M2_ENB = 32;   // ENB  – PWM speed
const int M2_IN3 = 33;   // IN3  – direction
const int M2_IN4 = 14;   // IN4  – direction

//  CUTTER motor – relay module
const int  CUTTER_PIN      = 13;   // Relay IN signal
const bool CUTTER_RELAY_ON = HIGH; // HIGH for active-HIGH modules; LOW for active-LOW

// If a motor runs the wrong way, set its flag to true
// (swaps IN1/IN2 or IN3/IN4 in software instead of rewiring).
const bool LEFT_INVERT  = false;
const bool RIGHT_INVERT = false;

// ── PWM ───────────────────────────────────────────────────
const int PWM_FREQ       = 5000;   // Hz
const int PWM_RESOLUTION = 8;      // bits → 0–255
// Core v3.x uses pin-based ledcAttach()/ledcWrite() – no channel numbers needed.

// Safety delay (ms) after stopping before changing motor direction
const int DIR_CHANGE_DELAY_MS = 50;

// Watchdog: cut motors if no /drive command received within this many ms
// (safety net if the phone disconnects while the mower is running)
const unsigned long WATCHDOG_MS = 2000;

// If the motor runs at full speed with no signal (active-LOW module:
// HIGH = off, LOW = full speed), set this to true.
// Set to false for standard active-HIGH modules.
const bool MOSFET_INVERT = true;

// ── Soft-start ramp (drive motors) ─────────────────────────────
const int RAMP_UP_STEP   = 20;   // PWM units per tick when accelerating (0–255)
const int RAMP_DOWN_STEP = 30;   // PWM units per tick when decelerating
const int RAMP_TICK_MS   = 10;   // ms between ramp ticks

// ── Runtime state ────────────────────────────────────────────
int  driveSpeed = 128;   // shared speed for both motors (0–255, default 50 %)

int  m1Speed  = 0;    // LEFT  – current PWM (0–255)
int  m1Target = 0;    // LEFT  – desired PWM (ramp steps toward this)
bool m1Fwd    = true; // LEFT  – current direction

int  m2Speed  = 0;    // RIGHT – current PWM
int  m2Target = 0;    // RIGHT – desired PWM
bool m2Fwd    = true; // RIGHT – current direction

unsigned long lastRampMs = 0;
unsigned long lastCmdMs  = 0;  // reset by every /drive command (watchdog)

bool cutterRelayOn = false;  // CUTTER – relay state

WebServer server(80);

// ── Web page (mobile-friendly D-pad UI) ──────────────────────
static const char HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1">
  <title>Robot Mower</title>
  <style>
    :root{
      --bg:#0d1117;--card:#161b22;--border:#30363d;
      --accent:#58a6ff;--green:#3fb950;--red:#f85149;--orange:#d29922;
      --text:#c9d1d9;--muted:#8b949e;
    }
    *{box-sizing:border-box;margin:0;padding:0}
    body{background:var(--bg);color:var(--text);
         font-family:-apple-system,Arial,sans-serif;
         padding:16px;max-width:400px;margin:auto;
         user-select:none;-webkit-user-select:none}
    h1{text-align:center;color:var(--accent);font-size:1.4rem;
       margin-bottom:6px;padding-top:8px}
    .sub{text-align:center;font-size:.85rem;color:var(--muted);margin-bottom:16px}
    #cmdlabel{font-weight:700;color:var(--accent)}
    .card{background:var(--card);border:1px solid var(--border);
          border-radius:10px;padding:16px;margin-bottom:14px}
    .srow{display:flex;justify-content:space-between;
          margin-bottom:8px;font-size:.85rem;color:var(--muted)}
    input[type=range]{width:100%;height:28px;accent-color:var(--accent)}
    /* D-pad */
    .dpad{display:grid;grid-template-columns:repeat(3,1fr);
          grid-template-rows:repeat(3,1fr);gap:10px;
          max-width:260px;margin:0 auto;aspect-ratio:1}
    .dpad button{border:none;border-radius:10px;font-size:1.7rem;
                 cursor:pointer;background:#21262d;color:var(--text);
                 transition:filter .1s;touch-action:none;
                 display:flex;align-items:center;justify-content:center}
    .dpad button:active,.dpad button.held{filter:brightness(.5)}
    #btn-fwd  {background:#132b13;color:var(--green)}
    #btn-bwd  {background:#2b1313;color:var(--red)}
    #btn-left {background:#13202b;color:var(--accent)}
    #btn-right{background:#13202b;color:var(--accent)}
    .btn-stp  {background:#2b2510 !important;color:var(--orange) !important}
    .empty    {background:transparent !important;pointer-events:none}
    /* Cutter */
    .ctrow{display:flex;align-items:center;gap:12px;margin-bottom:8px}
    #cutter-btn{width:100%;padding:12px;border:none;border-radius:8px;
                font-size:1rem;font-weight:700;cursor:pointer;touch-action:none;
                background:#132b13;color:var(--green)}
    #cutter-btn.on{background:#3fb950;color:#0d1117}
    /* Emergency stop */
    .estop{width:100%;padding:18px;background:#7f1d1d;color:#fff;
           border:2px solid #991b1b;border-radius:10px;font-size:1.1rem;
           font-weight:800;cursor:pointer;touch-action:none;letter-spacing:.04em}
    .estop:active{background:#991b1b}
  </style>
</head>
<body>
  <h1>&#129302; Robot Mower</h1>
  <p class="sub">Command: <span id="cmdlabel">STOP</span></p>

  <!-- Speed -->
  <div class="card">
    <div class="srow"><span>Speed</span><span id="sv">50 %</span></div>
    <input type="range" min="5" max="100" value="50" id="sl"
           oninput="setSpd(this.value)" onchange="setSpd(this.value)">
  </div>

  <!-- D-pad -->
  <div class="card">
    <div class="dpad">
      <div class="empty"></div>
      <button id="btn-fwd"
              onpointerdown="hold(event,'fwd')"   onpointerup="release(event)"
              onpointercancel="release(event)">&#9650;</button>
      <div class="empty"></div>

      <button id="btn-left"
              onpointerdown="hold(event,'left')"  onpointerup="release(event)"
              onpointercancel="release(event)">&#9664;</button>
      <button class="btn-stp"
              onpointerdown="release(event)">&#9632;</button>
      <button id="btn-right"
              onpointerdown="hold(event,'right')" onpointerup="release(event)"
              onpointercancel="release(event)">&#9654;</button>

      <div class="empty"></div>
      <button id="btn-bwd"
              onpointerdown="hold(event,'bwd')"   onpointerup="release(event)"
              onpointercancel="release(event)">&#9660;</button>
      <div class="empty"></div>
    </div>
  </div>

  <!-- Cutter -->
  <div class="card">
    <div class="srow"><span>&#9881; Cutter</span><span id="cv">OFF</span></div>
    <div class="ctrow">
      <button id="cutter-btn" onpointerdown="toggleCutter()">OFF</button>
    </div>
  </div>

  <button class="estop" onpointerdown="eStop()">
    &#9888;&nbsp; EMERGENCY STOP
  </button>

  <script>
    let holdTimer = null;
    let activeCmd = null;
    const REPEAT_MS = 150;  // re-send command while button is held

    function get(u){fetch(u).catch(()=>{})}

    function setSpd(v){
      document.getElementById('sv').innerText = v + ' %';
      get('/speed?val=' + v);
    }

    function sendCmd(cmd){
      document.getElementById('cmdlabel').innerText = cmd.toUpperCase();
      get('/drive?cmd=' + cmd);
    }

    function hold(e, cmd){
      e.currentTarget.setPointerCapture(e.pointerId);  // track pointer even if it drifts
      if(holdTimer) clearInterval(holdTimer);
      if(activeCmd){
        const prev = document.getElementById('btn-' + activeCmd);
        if(prev) prev.classList.remove('held');
      }
      activeCmd = cmd;
      document.getElementById('btn-' + cmd).classList.add('held');
      sendCmd(cmd);
      holdTimer = setInterval(() => sendCmd(cmd), REPEAT_MS);
    }

    function release(e){
      if(e) try{ e.currentTarget.releasePointerCapture(e.pointerId); }catch(_){}
      clearInterval(holdTimer);
      holdTimer = null;
      if(activeCmd){
        const el = document.getElementById('btn-' + activeCmd);
        if(el) el.classList.remove('held');
        activeCmd = null;
      }
      document.getElementById('cmdlabel').innerText = 'STOP';
      get('/drive?cmd=stop');
    }

    let cutterOn = false;

    function toggleCutter(){
      cutterOn = !cutterOn;
      const btn = document.getElementById('cutter-btn');
      btn.textContent = cutterOn ? 'ON' : 'OFF';
      btn.classList.toggle('on', cutterOn);
      document.getElementById('cv').innerText = cutterOn ? 'ON' : 'OFF';
      get('/cutter?val=' + (cutterOn ? '1' : '0'));
    }

    function eStop(){
      release(null);
      // Also stop cutter on emergency stop
      cutterOn = false;
      const btn = document.getElementById('cutter-btn');
      btn.textContent = 'OFF';
      btn.classList.remove('on');
      document.getElementById('cv').innerText = 'OFF';
      get('/cutter?val=0');
      get('/stop');
    }
  </script>
</body>
</html>
)rawliteral";

// ── Helpers ──────────────────────────────────────────────────
// Set L298N direction pins for one motor
static inline void setMotorDir(int in1, int in2, bool forward, bool invert) {
    bool fwd = forward ^ invert;
    digitalWrite(in1, fwd ? HIGH : LOW);
    digitalWrite(in2, fwd ? LOW  : HIGH);
}

// PWM write (L298N ENA/ENB: HIGH duty = more speed)
static inline void pwmWrite(int pin, int duty) {
    ledcWrite(pin, duty);
}

// Immediately cut a motor to 0 (bypasses ramp)
static inline void motorCut(int pin, int& speed, int& target) {
    speed  = 0;
    target = 0;
    pwmWrite(pin, 0);
}

// Non-blocking ramp – call every loop iteration
void rampMotors() {
    unsigned long now = millis();
    if (now - lastRampMs < (unsigned long)RAMP_TICK_MS) return;
    lastRampMs = now;

    auto step = [](int cur, int tgt) -> int {
        if (cur == tgt) return cur;
        int delta = (cur < tgt) ? RAMP_UP_STEP : -RAMP_DOWN_STEP;
        int next  = cur + delta;
        if ((delta > 0 && next > tgt) || (delta < 0 && next < tgt)) next = tgt;
        return constrain(next, 0, 255);
    };

    if (m1Speed != m1Target) {
        m1Speed = step(m1Speed, m1Target);
        pwmWrite(M1_ENA, m1Speed);
    }
    if (m2Speed != m2Target) {
        m2Speed = step(m2Speed, m2Target);
        pwmWrite(M2_ENB, m2Speed);
    }
}

// ── HTTP handlers ─────────────────────────────────────────────
void handleRoot() {
    server.sendHeader("Cache-Control", "no-cache");
    server.send_P(200, "text/html", HTML);
}

void handleSpeed() {
    if (!server.hasArg("val")) {
        server.send(400, "text/plain", "Bad Request");
        return;
    }
    int pct = constrain(server.arg("val").toInt(), 0, 100);
    driveSpeed = map(pct, 0, 100, 0, 255);

    // Update running motor targets immediately; ramp handles the transition
    if (m1Target > 0) m1Target = driveSpeed;
    if (m2Target > 0) m2Target = driveSpeed;

    server.send(200, "text/plain", "OK");
}

void handleCutter() {
    if (!server.hasArg("val")) {
        server.send(400, "text/plain", "Bad Request");
        return;
    }
    cutterRelayOn = server.arg("val").toInt() != 0;
    digitalWrite(CUTTER_PIN, cutterRelayOn ? CUTTER_RELAY_ON : !CUTTER_RELAY_ON);
    server.send(200, "text/plain", "OK");
}

void handleStop() {
    motorCut(M1_ENA, m1Speed, m1Target);
    motorCut(M2_ENB, m2Speed, m2Target);
    server.send(200, "text/plain", "OK");
}

void handleDrive() {
    if (!server.hasArg("cmd")) {
        server.send(400, "text/plain", "Bad Request");
        return;
    }
    String cmd = server.arg("cmd");

    // Whitelist valid commands to prevent injection
    if (cmd != "fwd" && cmd != "bwd" && cmd != "left" &&
        cmd != "right" && cmd != "stop") {
        server.send(400, "text/plain", "Invalid command");
        return;
    }

    lastCmdMs = millis();   // reset watchdog

    if (cmd == "stop") {
        m1Target = 0;
        m2Target = 0;
        server.send(200, "text/plain", "OK");
        return;
    }

    //   fwd   → left:fwd   right:fwd    (straight forward)
    //   bwd   → left:bwd   right:bwd    (straight backward)
    //   left  → left:fwd   right:bwd    (pivot left)
    //   right → left:bwd   right:fwd    (pivot right)
    bool newM1Fwd = (cmd == "fwd" || cmd == "left");
    bool newM2Fwd = (cmd == "fwd" || cmd == "right");

    // Change direction if needed – cut speed first to protect the H-bridge
    if (m1Fwd != newM1Fwd) {
        motorCut(M1_ENA, m1Speed, m1Target);
        delay(DIR_CHANGE_DELAY_MS);
        m1Fwd = newM1Fwd;
        setMotorDir(M1_IN1, M1_IN2, newM1Fwd, LEFT_INVERT);
    }
    if (m2Fwd != newM2Fwd) {
        motorCut(M2_ENB, m2Speed, m2Target);
        delay(DIR_CHANGE_DELAY_MS);
        m2Fwd = newM2Fwd;
        setMotorDir(M2_IN3, M2_IN4, newM2Fwd, RIGHT_INVERT);
    }

    m1Target = driveSpeed;
    m2Target = driveSpeed;

    server.send(200, "text/plain", "OK");
}

// Stop motors if the phone disconnects while the mower is moving
void checkWatchdog() {
    if ((m1Target > 0 || m2Target > 0) &&
        (millis() - lastCmdMs > WATCHDOG_MS)) {
        motorCut(M1_ENA, m1Speed, m1Target);
        motorCut(M2_ENB, m2Speed, m2Target);
        Serial.println("WATCHDOG: motors stopped");
    }
}

void handleNotFound() {
    server.send(404, "text/plain", "Not Found");
}

// ── Setup ─────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);

    // Direction pins – start both motors in forward direction
    pinMode(M1_IN1, OUTPUT); pinMode(M1_IN2, OUTPUT);
    pinMode(M2_IN3, OUTPUT); pinMode(M2_IN4, OUTPUT);
    setMotorDir(M1_IN1, M1_IN2, true, LEFT_INVERT);
    setMotorDir(M2_IN3, M2_IN4, true, RIGHT_INVERT);

    // Speed PWM (Core v3.x: pin-based, no channel numbers)
    ledcAttach(M1_ENA, PWM_FREQ, PWM_RESOLUTION);
    pwmWrite(M1_ENA, 0);

    ledcAttach(M2_ENB, PWM_FREQ, PWM_RESOLUTION);
    pwmWrite(M2_ENB, 0);

    // Cutter relay – ensure relay is off at startup
    pinMode(CUTTER_PIN, OUTPUT);
    digitalWrite(CUTTER_PIN, !CUTTER_RELAY_ON);

    // Start WiFi Access Point
    WiFi.softAP(AP_SSID, AP_PASSWORD);
    IPAddress ip = WiFi.softAPIP();
    Serial.printf("\nAP  SSID : %s\nAP  IP   : %s\nOpen http://%s in your browser\n",
                  AP_SSID, ip.toString().c_str(), ip.toString().c_str());

    // Register HTTP routes
    server.on("/",       HTTP_GET, handleRoot);
    server.on("/speed",  HTTP_GET, handleSpeed);
    server.on("/drive",  HTTP_GET, handleDrive);
    server.on("/stop",   HTTP_GET, handleStop);
    server.on("/cutter", HTTP_GET, handleCutter);
    server.onNotFound(handleNotFound);
    server.begin();
    Serial.println("HTTP server started.");
}

// ── Loop ──────────────────────────────────────────────────────
void loop() {
    server.handleClient();
    rampMotors();
    checkWatchdog();
}
