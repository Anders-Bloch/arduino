#include <AccelStepper.h>
#include <Servo.h>

// --- Machine constants ---
// 28BYJ-48 half-step: 4096 steps/rev; leadscrew: 10 mm/rev → 409.6 steps/mm
const float STEPS_PER_REV = 4096.0;
const float MM_PER_REV    = 10.0;
const float STEPS_PER_MM  = STEPS_PER_REV / MM_PER_REV;  // 409.6
const float MAX_SPEED_MM_S = 2.0;   // maximum axis speed in mm/s
const float ACCEL_MM_S2    = 1.0;   // acceleration in mm/s²

// --- Pen-lift servo (MG90S, pin 7) ---
// Tune these two angles to match your physical mount
const int PEN_SERVO_PIN  = 7;
const int PEN_DOWN_ANGLE = 90;  // degrees — pen touches PCB
const int PEN_UP_ANGLE   = 60;  // degrees — pen clear of PCB

Servo penServo;

void penDown() { penServo.write(PEN_DOWN_ANGLE); delay(150); }
void penUp()   { penServo.write(PEN_UP_ANGLE);   delay(150); }

// --- Stepper motors ---
// X-axis: moves the pen left/right — IN1=8, IN2=10, IN3=9, IN4=11
AccelStepper stepperX(AccelStepper::HALF4WIRE, 8, 10, 9, 11);
// Y-axis: moves the PCB board forward/backward — IN1=2, IN2=4, IN3=3, IN4=5
// Because the board moves (not the pen), a positive board movement carries the
// PCB away from the pen, so Y steps must be negated to match G-code coordinates.
// If the Y direction is wrong after assembly, flip Y_INVERT to false.
const bool Y_INVERT = true;
AccelStepper stepperY(AccelStepper::HALF4WIRE, 2, 4, 3, 5);

long mmToSteps(float mm) { return (long)(mm * STEPS_PER_MM); }

// Synchronised move: both axes start and finish at the same time.
// The longer axis runs at MAX_SPEED_MM_S; the shorter axis is slowed
// proportionally so the travel time is identical for both.
void stageMoveTo(float xMm, float yMm) {
  float dx = abs(xMm - curX);
  float dy = abs(yMm - curY);
  float dominant = max(dx, dy);

  float speedX = (dominant > 0.0) ? MAX_SPEED_MM_S * (dx / dominant) : MAX_SPEED_MM_S;
  float speedY = (dominant > 0.0) ? MAX_SPEED_MM_S * (dy / dominant) : MAX_SPEED_MM_S;

  // Acceleration scales with speed so the ramp profile stays proportional
  float accelX = ACCEL_MM_S2 * (dx / dominant > 0.0 ? dx / dominant : 1.0);
  float accelY = ACCEL_MM_S2 * (dy / dominant > 0.0 ? dy / dominant : 1.0);

  stepperX.setMaxSpeed(max(speedX, 0.01f) * STEPS_PER_MM);
  stepperX.setAcceleration(max(accelX, 0.01f) * STEPS_PER_MM);
  stepperY.setMaxSpeed(max(speedY, 0.01f) * STEPS_PER_MM);
  stepperY.setAcceleration(max(accelY, 0.01f) * STEPS_PER_MM);

  stepperX.moveTo(mmToSteps(xMm));
  stepperY.moveTo(mmToSteps(Y_INVERT ? -yMm : yMm));
}

void waitForMove() {
  while (stepperX.distanceToGo() != 0 || stepperY.distanceToGo() != 0) {
    stepperX.run();
    stepperY.run();
  }
}

// --- G-code parser ---
// Current machine position in mm (absolute coordinates)
float curX        = 0.0;
float curY        = 0.0;
bool  absoluteMode = true;  // G90 = absolute (default), G91 = relative

// Return the numeric value following 'param' in 'line', or defaultVal if absent
float parseParam(const char* line, char param, float defaultVal) {
  const char* p = strchr(line, param);
  if (p == nullptr) return defaultVal;
  return atof(p + 1);
}

void processLine(const char* rawLine) {
  // Skip blank lines and full-line comments
  if (rawLine[0] == '\0' || rawLine[0] == ';' || rawLine[0] == '(') {
    Serial.println("ok");
    return;
  }

  // Copy into a mutable buffer and strip inline ';' comments
  char buf[64];
  strncpy(buf, rawLine, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';
  char* semi = strchr(buf, ';');
  if (semi) *semi = '\0';

  // Uppercase for simpler matching
  for (int i = 0; buf[i]; i++) buf[i] = toupper((unsigned char)buf[i]);

  // --- G-codes ---
  const char* gp = strchr(buf, 'G');
  if (gp) {
    int gCode = atoi(gp + 1);

    if (gCode == 90) { absoluteMode = true;  Serial.println("ok"); return; }
    if (gCode == 91) { absoluteMode = false; Serial.println("ok"); return; }

    // G28 — Move to home (X0, Y0) with pen up
    if (gCode == 28) {
      penUp();
      stageMoveTo(0.0, 0.0);
      waitForMove();
      curX = 0.0;
      curY = 0.0;
      Serial.println("ok");
      return;
    }

    // G92 — Set current position as origin without moving
    if (gCode == 92) {
      stepperX.setCurrentPosition(0);
      stepperY.setCurrentPosition(0);
      curX = 0.0;
      curY = 0.0;
      Serial.println("ok");
      return;
    }

    if (gCode == 0 || gCode == 1) {
      float xVal = parseParam(buf, 'X', absoluteMode ? curX : 0.0f);
      float yVal = parseParam(buf, 'Y', absoluteMode ? curY : 0.0f);

      float targetX = absoluteMode ? xVal : curX + xVal;
      float targetY = absoluteMode ? yVal : curY + yVal;

      if (gCode == 0) penUp();    // rapid move — pen up
      else            penDown();  // draw move  — pen down

      stageMoveTo(targetX, targetY);
      waitForMove();

      curX = targetX;
      curY = targetY;
      Serial.println("ok");
      return;
    }
  }

  // --- M-codes ---
  const char* mp = strchr(buf, 'M');
  if (mp) {
    int mCode = atoi(mp + 1);
    if (mCode == 2 || mCode == 30) {  // program end
      penUp();
      Serial.println("ok");
      Serial.println("Program end");
      return;
    }
  }

  // Unknown command — acknowledge anyway so host does not stall
  Serial.println("ok");
}

// --- Serial line buffer ---
char lineBuf[64];
int  lineLen = 0;

void setup() {
  // 115200 baud for faster G-code streaming
  Serial.begin(115200);

  penServo.attach(PEN_SERVO_PIN);
  penUp();

  stepperX.setMaxSpeed(STEPS_PER_MM * MAX_SPEED_MM_S);
  stepperX.setAcceleration(STEPS_PER_MM * ACCEL_MM_S2);
  stepperY.setMaxSpeed(STEPS_PER_MM * MAX_SPEED_MM_S);
  stepperY.setAcceleration(STEPS_PER_MM * ACCEL_MM_S2);

  Serial.println("PCB plotter ready — send G-code lines, wait for 'ok'");
}

void loop() {
  // Read incoming characters and process complete lines
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (lineLen > 0) {
        lineBuf[lineLen] = '\0';
        processLine(lineBuf);
        lineLen = 0;
      }
    } else if (lineLen < (int)sizeof(lineBuf) - 1) {
      lineBuf[lineLen++] = c;
    }
  }
}
