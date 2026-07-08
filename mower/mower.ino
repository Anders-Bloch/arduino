/**
 * ESP32 Robot Mower Controller
 * ============================
 * Differential-drive robot mower using two 31zy 12 V DC motors:
 *   - MOSFET PWM driver modules  → speed (0–100 %)
 *   - Single-channel relay modules → direction (forward / reverse)
 *
 * Access Point mode – no router needed.
 * 1. Connect your phone to the "MowerControl" WiFi network.
 * 2. Open http://192.168.4.1 in the browser.
 * 3. Hold a direction button to drive; release to stop.
 *    A 2-second watchdog cuts the motors if the connection drops.
 *
 * Wiring guide
 * ──────────────────────────────────────────────────────────────
 *  LEFT wheel motor
 *    ESP32 GPIO 25  → MOSFET module  PWM/SIG input
 *    ESP32 GPIO 26  → Relay module   IN  pin
 *    Relay NO + COM → left motor terminals (swaps polarity for reverse)
 *
 *  RIGHT wheel motor
 *    ESP32 GPIO 32  → MOSFET module  PWM/SIG input
 *    ESP32 GPIO 33  → Relay module   IN  pin
 *    Relay NO + COM → right motor terminals
 *
 *  MOSFET VCC / Relay VCC → ESP32 5 V (or external 5 V)
 *  All GNDs must share a common ground with the ESP32.
 *
 * Compatible with ESP32 Arduino Core v3.x
 * Board in Arduino IDE: "ESP32 Dev Module"
 * ──────────────────────────────────────────────────────────────
 */

#include <WiFi.h>
#include <WebServer.h>

// ── WiFi Access Point ────────────────────────────────────────
const char* AP_SSID     = "MowerControl";
const char* AP_PASSWORD = "mower1234";   // ≥8 chars; use "" for open network

// ── Pin assignments  (adjust to your wiring) ─────────────────
//  LEFT wheel motor
const int M1_PWM_PIN = 25;   // MOSFET signal
const int M1_DIR_PIN = 26;   // Relay IN

//  RIGHT wheel motor
const int M2_PWM_PIN = 32;   // MOSFET signal
const int M2_DIR_PIN = 33;   // Relay IN

// Most optocoupler relay modules activate on LOW.
// Change to HIGH if your module activates on HIGH.
const int RELAY_ACTIVE_LEVEL = LOW;

// If a wheel spins the wrong way for "forward", set its flag to true
// instead of rewiring the motor terminals.
const bool LEFT_INVERT  = false;
const bool RIGHT_INVERT = false;

// ── PWM ─────────────────────────────────────────────────────
const int PWM_FREQ       = 5000;   // Hz
const int PWM_RESOLUTION = 8;      // bits → 0–255
// Core v3.x uses pin-based ledcAttach()/ledcWrite() – no channel numbers needed.

// Safety delay (ms) between stopping motor and switching relay direction
const int DIR_CHANGE_DELAY_MS = 80;

// Watchdog: cut motors if no /drive command received within this many ms
// (safety net if the phone disconnects while the mower is running)
const unsigned long WATCHDOG_MS = 2000;

// If the motor runs at full speed with no signal (active-LOW module:
// HIGH = off, LOW = full speed), set this to true.
// Set to false for standard active-HIGH modules.
const bool MOSFET_INVERT = true;

// ── Soft-start ramp ──────────────────────────────────────────
const int RAMP_UP_STEP   = 3;    // PWM units per tick when accelerating (0–255)
const int RAMP_DOWN_STEP = 6;    // PWM units per tick when decelerating
const int RAMP_TICK_MS   = 15;   // ms between ramp ticks

// ── Runtime state ────────────────────────────────────────────
int  driveSpeed = 128;   // shared speed for both motors (0–255, default 50 %)

int  m1Speed  = 0;    // LEFT  – current PWM (0–255)
int  m1Target = 0;    // LEFT  – desired PWM (ramp steps toward this)
bool m1Fwd    = true; // LEFT  – current relay direction

int  m2Speed  = 0;    // RIGHT – current PWM
int  m2Target = 0;    // RIGHT – desired PWM
bool m2Fwd    = true; // RIGHT – current relay direction

unsigned long lastRampMs = 0;
unsigned long lastCmdMs  = 0;  // reset by every /drive command (watchdog)

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

  <button class="estop" onpointerdown="eStop()">
    &#9888;&nbsp; EMERGENCY STOP
  </button>

  <script>
    let holdTimer = null;
    let activeCmd = null;
    const REPEAT_MS = 350;  // re-send command while button is held

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

    function eStop(){
      release(null);
      get('/stop');
    }
  </script>
</body>
</html>
)rawliteral";

// ── Helpers ──────────────────────────────────────────────────
static inline void relaySet(int pin, bool activate) {
    digitalWrite(pin, activate ? RELAY_ACTIVE_LEVEL : !RELAY_ACTIVE_LEVEL);
}

// PWM write – inverts duty cycle for active-LOW MOSFET modules
static inline void pwmWrite(int pin, int duty) {
    ledcWrite(pin, MOSFET_INVERT ? (255 - duty) : duty);
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
        pwmWrite(M1_PWM_PIN, m1Speed);
    }
    if (m2Speed != m2Target) {
        m2Speed = step(m2Speed, m2Target);
        pwmWrite(M2_PWM_PIN, m2Speed);
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

void handleStop() {
    if (!server.hasArg("motor")) {
        // No motor specified – stop both
        motorCut(M1_PWM_PIN, m1Speed, m1Target);
        motorCut(M2_PWM_PIN, m2Speed, m2Target);
    } else {
        int motor = server.arg("motor").toInt();
        if (motor == 1)      motorCut(M1_PWM_PIN, m1Speed, m1Target);
        else if (motor == 2) motorCut(M2_PWM_PIN, m2Speed, m2Target);
        else { server.send(400, "text/plain", "Invalid motor"); return; }
    }
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

    // Map drive command to per-motor forward/backward intent:
    //   fwd   → left:fwd   right:fwd    (straight forward)
    //   bwd   → left:bwd   right:bwd    (straight backward)
    //   left  → left:bwd   right:fwd    (pivot left)
    //   right → left:fwd   right:bwd    (pivot right)
    bool newM1Fwd = (cmd == "fwd" || cmd == "right");
    bool newM2Fwd = (cmd == "fwd" || cmd == "left");

    // Switch relay if direction changed – always stop first
    if (m1Fwd != newM1Fwd) {
        motorCut(M1_PWM_PIN, m1Speed, m1Target);
        delay(DIR_CHANGE_DELAY_MS);
        m1Fwd = newM1Fwd;
        relaySet(M1_DIR_PIN, !(newM1Fwd ^ LEFT_INVERT));
    }
    if (m2Fwd != newM2Fwd) {
        motorCut(M2_PWM_PIN, m2Speed, m2Target);
        delay(DIR_CHANGE_DELAY_MS);
        m2Fwd = newM2Fwd;
        relaySet(M2_DIR_PIN, !(newM2Fwd ^ RIGHT_INVERT));
    }

    // Set speed targets – rampMotors() steps toward them each tick
    m1Target = driveSpeed;
    m2Target = driveSpeed;

    server.send(200, "text/plain", "OK");
}

// Stop motors if the phone disconnects while the mower is moving
void checkWatchdog() {
    if ((m1Target > 0 || m2Target > 0) &&
        (millis() - lastCmdMs > WATCHDOG_MS)) {
        motorCut(M1_PWM_PIN, m1Speed, m1Target);
        motorCut(M2_PWM_PIN, m2Speed, m2Target);
        Serial.println("WATCHDOG: motors stopped");
    }
}

void handleNotFound() {
    server.send(404, "text/plain", "Not Found");
}

// ── Setup ─────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);

    // Relay pins – start in forward direction (respects INVERT flags)
    pinMode(M1_DIR_PIN, OUTPUT);
    pinMode(M2_DIR_PIN, OUTPUT);
    relaySet(M1_DIR_PIN, !(true ^ LEFT_INVERT));
    relaySet(M2_DIR_PIN, !(true ^ RIGHT_INVERT));

    // PWM channels (Core v3.x: pin-based, no channel numbers)
    ledcAttach(M1_PWM_PIN, PWM_FREQ, PWM_RESOLUTION);
    pwmWrite(M1_PWM_PIN, 0);

    ledcAttach(M2_PWM_PIN, PWM_FREQ, PWM_RESOLUTION);
    pwmWrite(M2_PWM_PIN, 0);

    // Start WiFi Access Point
    WiFi.softAP(AP_SSID, AP_PASSWORD);
    IPAddress ip = WiFi.softAPIP();
    Serial.printf("\nAP  SSID : %s\nAP  IP   : %s\nOpen http://%s in your browser\n",
                  AP_SSID, ip.toString().c_str(), ip.toString().c_str());

    // Register HTTP routes
    server.on("/",      HTTP_GET, handleRoot);
    server.on("/speed", HTTP_GET, handleSpeed);
    server.on("/drive", HTTP_GET, handleDrive);
    server.on("/stop",  HTTP_GET, handleStop);
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
