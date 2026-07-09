# Wiring Guide — ESP32 Robot Mower

## Components

| Qty | Component | Role |
|-----|-----------|------|
| 1 | ESP32 Dev Kit | Controller |
| 2 | 31zy 12 V DC motor | Left & right wheel drive |
| 2 | XY-FET / Dual MOSFET PWM module (6-pin) | Speed control (PWM) |
| 4 | Single-channel 5 V relay module (RUNCCI-YUN) | Direction control — 2 per motor, H-bridge |
| 1 | 12 V DC power supply / battery | Motor power |
| 1 | 5 V supply or ESP32 5 V pin | Logic / relay coil power |

---

## How the circuit works

Each motor uses **two SPDT relays wired as a DPDT H-bridge** for direction, and **one MOSFET module** for speed.

- **Relay A** routes the MOSFET output (+PWM) to either Motor Terminal A or B.  
- **Relay B** routes Battery − (GND) to the *opposite* terminal.  
- Both relay coils are driven from the **same ESP32 direction GPIO** (wired in parallel), so they always switch together.

```
Forward (both relays OFF — NO contacts active):
  Battery + → MOSFET (PWM) → OUT+ → Relay-A NO → COM → Motor Terminal A (+)
  Motor Terminal B (−) → Relay-B COM → NO → Battery −

Reverse (both relays ON — NC contacts active):
  Battery + → MOSFET (PWM) → OUT+ → Relay-B NC → COM → Motor Terminal B (+)
  Motor Terminal A (−) → Relay-A COM → NC → Battery −
```

The MOSFET is always in the current path, so **PWM speed control works in both directions**.  
No battery voltage is switched through the relay contacts — only the MOSFET output and GND.

---

## Pin assignments (matches mower.ino)

| Signal | ESP32 GPIO | Connected to |
|--------|-----------|--------------|
| Left motor PWM | 25 | MOSFET-1 TRIG/PWM |
| Left motor DIR | 26 | Relay-A1 IN **and** Relay-B1 IN (parallel) |
| Right motor PWM | 32 | MOSFET-2 TRIG/PWM |
| Right motor DIR | 33 | Relay-A2 IN **and** Relay-B2 IN (parallel) |

---

## Wiring — one motor (repeat for second motor)

### 1. MOSFET PWM module (6-pin)

| Pin | Connect to | Notes |
|-----|-----------|-------|
| `TRIG` / `PWM` | ESP32 GPIO 25 (left) / GPIO 32 (right) | PWM speed signal — 3.3 V logic |
| `GND` | Common GND | Signal ground |
| `VIN−` | Battery − (12 V−) | Power supply negative |
| `VIN+` | Battery + (12 V+) | Power supply positive |
| `OUT+` | Relay-A NO **and** Relay-B NC | Switched positive rail — feeds both relays |
| `OUT−` | Battery − / Common GND | Power return (same node as VIN−) |

```
 MOSFET module (6-pin)
 ┌──────────────┐
 │ TRIG / PWM   ├───── ESP32 GPIO 25 (left) / GPIO 32 (right)
 │ GND          ├───── Common GND
 │ VIN−         ├───── Battery − (12 V−)
 │ VIN+         ├───── Battery + (12 V+)
 │ OUT+         ├───── Relay-A NO  (and Relay-B NC)
 │ OUT−         ├───── Battery − (Common GND)
 └──────────────┘
```

---

### 2. Relay A — switches the positive (MOSFET) rail

Both relay coils (A and B) share the same ESP32 direction GPIO.

```
 Relay-A
 ┌─────────────┐
 │ VCC         ├───── 5 V
 │ GND         ├───── Common GND
 │ IN          ├───── ESP32 GPIO 26 (left) / GPIO 33 (right)
 │             │      ← also connects to Relay-B IN
 │ COM         ├───── Motor Terminal A
 │ NO          ├───── MOSFET OUT+             [relays OFF = forward: A gets +PWM]
 │ NC          ├───── Battery − (Common GND)  [relays ON  = reverse: A is return]
 └─────────────┘
```

---

### 3. Relay B — switches the negative (GND) return rail

```
 Relay-B
 ┌─────────────┐
 │ VCC         ├───── 5 V
 │ GND         ├───── Common GND
 │ IN          ├───── ESP32 GPIO 26 (left) / GPIO 33 (right)
 │             │      ← same pin as Relay-A IN
 │ COM         ├───── Motor Terminal B
 │ NO          ├───── Battery − (Common GND)  [relays OFF = forward: B is return]
 │ NC          ├───── MOSFET OUT+             [relays ON  = reverse: B gets +PWM]
 └─────────────┘
```

---

### 4. Motor

```
 DC Motor
 ┌──────────┐
 │ Terminal A├───── Relay-A COM
 │ Terminal B├───── Relay-B COM
 └──────────┘
```

If the motor spins the wrong way for forward, set `LEFT_INVERT = true`
(or `RIGHT_INVERT = true`) in `mower.ino` — no rewiring needed.

---

## Full system wiring diagram (both motors)

```
12 V Battery + ──────────────────────────────┐
12 V Battery − ──────────────────────────┐   │
                                         │   │
    ┌──────── LEFT MOTOR ──────────────┐ │   │
    │                                  │ │   │
    │  MOSFET-1                        │ │   │
    │  VIN+  ──────────────────────────┼─┼───┤ Battery +
    │  VIN−  ──────────────────────────┼─┤   │
    │  GND   ──────────────────────────┼─┤   │
    │  TRIG  ── ESP32 GPIO 25          │ │   │
    │  OUT+  ──────────┐               │ │   │
    │  OUT−  ──────────┼───────────────┼─┤   │
    │                  │               │ │   │
    │  Relay-A1        │               │ │   │
    │  VCC   ── 5V     │               │ │   │
    │  GND   ──────────┼───────────────┼─┤   │
    │  IN    ── ESP32 GPIO 26 ─────────┼─┼─┐ │
    │  COM   ──────────┘               │ │ │ │
    │  NO    ── Motor-L Terminal A ─┐  │ │ │ │
    │  NC    ── Motor-L Terminal B ─┼┐ │ │ │ │
    │                               ││ │ │ │ │
    │  Relay-B1                     ││ │ │ │ │
    │  VCC   ── 5V                  ││ │ │ │ │
    │  GND   ──────────────────────────┤ │ │ │
    │  IN    ── ESP32 GPIO 26 (same as Relay-A1)
    │  COM   ──────────────────────────┤ │ │ │
    │  NO    ── Motor-L Terminal B ─┘│ │ │ │ │
    │  NC    ── Motor-L Terminal A ──┘ │ │ │ │
    │                                  │ │ │ │
    └──────────────────────────────────┘ │ │ │
                                         │ │ │
    ┌──────── RIGHT MOTOR ────────────┐  │ │ │
    │  (same structure, GPIO 32 & 33) │  │ │ │
    └──────────────────────────────────┘ │ │ │
                                         │ │ │
Common GND ──────────────────────────── ─┘ │ │
  (ESP32 GND, all MOSFET GND/VIN−,         │ │
   all Relay GND, all Relay-B COM,         │ │
   Battery −)                              │ │
                                           │ │
All direction GPIO 26/33 ──────────────────┘ │
All MOSFET VIN+ / Battery + ─────────────────┘
```

---

## Relay logic

| GPIO state | Both relays | Active contacts | Direction |
|-----------|-------------|-----------------|-----------|
| **HIGH** | OFF | NO | **Forward**: A = +PWM, B = GND |
| **LOW** | ON | NC | **Reverse**: B = +PWM, A = GND |

Relay modules are **active LOW** (RUNCCI-YUN default).
Change `RELAY_ACTIVE_LEVEL = HIGH` in `mower.ino` if yours activate on HIGH.

---

## Power supply notes

| Rail | Source | Powers |
|------|--------|--------|
| 12 V | Battery / PSU | Motors, MOSFET VIN+ |
| 5 V | ESP32 5V pin or regulator | All relay coils (VCC) |
| 3.3 V | ESP32 internal | ESP32 logic only |

- **Common ground is essential.** ESP32 GND, all MOSFET GND/VIN−, all Relay GND, all Relay-B COM, and Battery − must share one node.
- The ESP32 can be powered from 12 V via a buck converter (→ 5 V to `Vin`). Do **not** connect 12 V directly to ESP32 pins.
- MOSFET `TRIG` and Relay `IN` pins accept 3.3 V logic from ESP32 directly.

---

## Pre-power checklist

- [ ] All GND connections joined to a single common ground point
- [ ] No 12 V connected to ESP32 power pins
- [ ] MOSFET-1 TRIG → GPIO 25,  MOSFET-2 TRIG → GPIO 32
- [ ] Relay-A1 IN + Relay-B1 IN both wired to GPIO 26
- [ ] Relay-A2 IN + Relay-B2 IN both wired to GPIO 33
- [ ] Relay-A COM → Motor Terminal A
- [ ] Relay-B COM → Motor Terminal B
- [ ] Relay-A NO and Relay-B NC → MOSFET OUT+ (same node)
- [ ] Relay-A NC and Relay-B NO → Battery − / Common GND (same node)
- [ ] Motor terminals A and B are **not** shorted together through any direct wire
- [ ] `RELAY_ACTIVE_LEVEL` in `mower.ino` matches your relay trigger level (default LOW)
- [ ] If a wheel spins backwards for "forward", set `LEFT_INVERT`/`RIGHT_INVERT = true` in `mower.ino`
- [ ] `MOSFET_INVERT = true` in `mower.ino` if motors run at full speed without a signal
