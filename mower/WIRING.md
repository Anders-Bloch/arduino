# Wiring Guide — ESP32 Robot Mower

## Components

| Qty | Component | Role |
|-----|-----------|------|
| 1 | ESP32 Dev Kit | Controller |
| 2 | 31zy 12 V DC motor | Left & right wheel drive |
| 2 | XY-FET / Dual MOSFET PWM module | Speed control (PWM) |
| 2 | Single-channel 5 V relay module (RUNCCI-YUN) | Direction control (polarity switch) |
| 1 | 12 V DC power supply / battery | Motor power |
| 1 | 5 V supply or ESP32 5 V pin | Logic/relay coil power |

---

## How the circuit works

The **MOSFET module** sits in series with the motor power path and controls **how much current** flows using PWM — it sets speed.

The **relay module** switches which motor terminal sees the **positive rail vs. the negative (GND) rail** — it sets direction.

```
Forward (relay OFF — NO contact active):
  Battery + → MOSFET (PWM) → Motor A (+) → ~~motor~~ → Motor B (−) → Relay COM → NO → Battery −

Reverse (relay ON — NC contact active):
  Battery + → Relay NC → COM → Motor B (+) → ~~motor~~ → Motor A (−) → MOSFET (PWM) → Battery −
```

PWM speed control works in **both** directions because current always passes through the MOSFET module.

---

## Pin assignments (matches mower.ino)

| Signal | ESP32 GPIO | Connected to |
|--------|-----------|--------------|
| Left motor PWM | 25 | MOSFET module 1 — SIG |
| Left motor DIR | 26 | Relay module 1 — IN |
| Right motor PWM | 32 | MOSFET module 2 — SIG |
| Right motor DIR | 33 | Relay module 2 — IN |

---

## Wiring — one motor (repeat for second motor)

### 1. MOSFET PWM module (6-pin)

Your module has the following pins:

| Pin | Connect to | Notes |
|-----|-----------|-------|
| `TRIG` / `PWM` | ESP32 GPIO 25 (left) / GPIO 32 (right) | PWM speed signal — 3.3 V logic is sufficient |
| `GND` | Common GND (Battery − and ESP32 GND) | Signal ground |
| `VIN−` | Battery − (12 V−) | Power supply negative |
| `VIN+` | Battery + (12 V+) | Power supply positive |
| `OUT+` | Motor Terminal A | Switched positive rail to motor |
| `OUT−` | Relay module COM | Return path — switched by relay for direction |

```
 MOSFET module (6-pin)
 ┌──────────────┐
 │ TRIG / PWM   ├───── ESP32 GPIO 25 (left) / GPIO 32 (right)
 │              │
 │ GND          ├───── Common GND
 │              │
 │ VIN−         ├───── Battery − (12 V−)
 │              │
 │ VIN+         ├───── Battery + (12 V+)
 │              │
 │ OUT+         ├───── Motor Terminal A
 │              │
 │ OUT−         ├───── Relay COM  ──────────── (see relay section below)
 └──────────────┘
```

Current flow through the module:
- **Forward:** `VIN+` → internal MOSFETs (PWM gated) → `OUT+` → Motor A → motor → Motor B → `OUT−` → Relay → Battery −
- **Reverse:** Battery + → Relay NC → `OUT−` → motor → `OUT+` → MOSFETs → `VIN−` → Battery −

---

### 2. Relay module

```
 Relay module
 ┌─────────────┐
 │ VCC         ├───── 5 V  (ESP32 5V pin or external 5V)
 │             │
 │ GND         ├───── Common GND
 │             │
 │ IN          ├───── ESP32 GPIO 26  (left)  / GPIO 33  (right)
 │             │
 │ COM         ├───── Motor Terminal B
 │             │
 │ NO          ├───── 12 V Battery (−)  ← GND             [FORWARD path]
 │             │
 │ NC          ├───── 12 V Battery (+)                     [REVERSE path]
 └─────────────┘
```

> The **NC contact carries full 12 V battery voltage.** These relay modules
> are rated for 30 V DC on the switching contacts — 12 V is well within spec.

---

### 3. Motor

```
 DC Motor
 ┌──────────┐
 │ Terminal A├───── MOSFET OUT  (as above)
 │ Terminal B├───── Relay COM   (as above)
 └──────────┘
```

If the motor spins the **wrong way for forward**, either:
- Swap Terminal A and Terminal B, **or**
- Set `LEFT_INVERT = true` (or `RIGHT_INVERT = true`) in `mower.ino` — no rewiring needed.

---

## Full system wiring diagram (both motors)

```
12 V Battery +──────────────────────────────────────────────────────────┐
12 V Battery −──────────────────────────────────────────────────────┐   │
                                                                     │   │
              ┌──────────────────────────────────────────────────┐  │   │
              │           LEFT MOTOR CIRCUIT                     │  │   │
              │                                                   │  │   │
              │  ┌─ MOSFET-1 (6-pin) ──┐                        │  │   │
              │  │ VIN+                ├── Battery +             │  │   │
              │  │ VIN−                ├── Battery −             │  │   │
              │  │ GND                 ├── Common GND            │  │   │
              │  │ TRIG/PWM            ├── ESP32 GPIO 25         │  │   │
              │  │ OUT+                ├── Motor-L Terminal A    │  │   │
              │  │ OUT−                ├──────────────────────┐  │  │   │
              │  └─────────────────────┘                      │  │  │   │
              │                                                │  │  │   │
              │  ┌─ RELAY-1 ───┐                              │  │  │   │
              │  │ VCC         ├── 5 V                        │  │  │   │
              │  │ GND         ├── Common GND                 │  │  │   │
              │  │ IN          ├── ESP32 GPIO 26              │  │  │   │
              │  │ COM         ├──────────────────────────────┘  │  │   │
              │  │ NO          ├── Battery − (GND) ──────────────┤  │   │
              │  │ NC          ├── Battery + (12 V) ─────────────┼──┼───┤
              │  └─────────────┘                                 │  │   │
              └──────────────────────────────────────────────────┘  │   │
                                                                     │   │
              ┌──────────────────────────────────────────────────┐  │   │
              │           RIGHT MOTOR CIRCUIT                    │  │   │
              │                                                   │  │   │
              │  ┌─ MOSFET-2 (6-pin) ──┐                        │  │   │
              │  │ VIN+                ├── Battery +             │  │   │
              │  │ VIN−                ├── Battery −             │  │   │
              │  │ GND                 ├── Common GND            │  │   │
              │  │ TRIG/PWM            ├── ESP32 GPIO 32         │  │   │
              │  │ OUT+                ├── Motor-R Terminal A    │  │   │
              │  │ OUT−                ├──────────────────────┐  │  │   │
              │  └─────────────────────┘                      │  │  │   │
              │                                                │  │  │   │
              │  ┌─ RELAY-2 ───┐                              │  │  │   │
              │  │ VCC         ├── 5 V                        │  │  │   │
              │  │ GND         ├── Common GND                 │  │  │   │
              │  │ IN          ├── ESP32 GPIO 33              │  │  │   │
              │  │ COM         ├──────────────────────────────┘  │  │   │
              │  │ NO          ├── Battery − (GND) ──────────────┘  │   │
              │  │ NC          ├── Battery + (12 V) ────────────────┘   │
              │  └─────────────┘                                        │
              └──────────────────────────────────────────────────────┘

Common GND node ──── ESP32 GND
                ──── MOSFET-1 VIN− and GND
                ──── MOSFET-2 VIN− and GND
                ──── RELAY-1  GND and NO
                ──── RELAY-2  GND and NO
                ──── Battery −
```

---

## Relay logic

| Relay IN pin | Relay state | Contact | Motor direction |
|-------------|-------------|---------|-----------------|
| HIGH (or floating) | OFF | NO active | **Forward** — Motor B → GND |
| LOW | ON | NC active | **Reverse** — Motor B → Battery+ |

The relay modules in this project are **active LOW** (optocoupler pulls IN to GND to activate).
This is the default for RUNCCI-YUN and most similar modules.
If your module activates on HIGH, change `RELAY_ACTIVE_LEVEL = HIGH` in `mower.ino`.

---

## Power supply notes

| Rail | Source | Powers |
|------|--------|--------|
| 12 V | Battery / PSU | Motors, MOSFET module VCC, Relay NC contact |
| 5 V | ESP32 Vin/5V pin (or separate regulator) | Relay coils (VCC) |
| 3.3 V | ESP32 internal | ESP32 logic only |

- **Common ground is essential.** ESP32 GND, MOSFET GND, Relay GND, and Battery − must all be connected together.
- The ESP32 itself can be powered from the 12 V battery via a buck converter (→ 5 V to `Vin`).
- Do **not** connect 12 V directly to the ESP32 3.3 V or 5 V pins.
- The MOSFET module `SIG` pin accepts 3.3 V logic from the ESP32 directly.
- The relay `IN` pin is driven through an internal optocoupler — 3.3 V from the ESP32 is sufficient.

---

## Pre-power checklist

- [ ] All GND connections joined to a single common ground point
- [ ] No 12 V connected to ESP32 power pins (use a buck converter)
- [ ] MOSFET SIG pins connected to correct ESP32 PWM-capable GPIOs (25, 32)
- [ ] Relay IN pins connected to correct ESP32 GPIOs (26, 33)
- [ ] Relay NO terminals connected to Battery − (GND), **not** Battery +
- [ ] Relay NC terminals connected to Battery + (12 V), **not** GND
- [ ] Motor terminals A and B not shorted together
- [ ] `RELAY_ACTIVE_LEVEL` in `mower.ino` matches your relay module's trigger level
- [ ] If a wheel spins backwards for "forward", set `LEFT_INVERT`/`RIGHT_INVERT = true` in `mower.ino`
