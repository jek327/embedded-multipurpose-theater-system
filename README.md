# theater-lighting-fsm

Embedded theater lighting control system built on the **TM4C123GH6PM (Tiva C)** microcontroller. Uses a Moore-style Finite State Machine to manage four operating modes — OFF, HOUSE, SPEAKER, and MUSIC — driven by GPIO interrupts and an IR sensor.

---

## Overview

This project implements a prototype theater control system in C for an embedded systems course. The system responds to two push buttons and an IR sensor to transition between lighting states, with LED outputs representing house lights, a spotlight, and a music visualization.

---

## Hardware

| Component | Description |
|---|---|
| TM4C123GH6PM | Tiva C Series microcontroller |
| 4x LEDs | House light, spotlight, and 2 music visualization LEDs |
| IR Sensor | Detects presence of a speaker on stage (active low) |
| Breadboard + jumper wires | Circuit connections |
| SW1 (PF4) | Left button — speaker mode |
| SW2 (PF0) | Right button — music mode |

### Pin Mapping

| Pin | Role |
|---|---|
| PE1 | House LED |
| PE2 | Spotlight LED / Music LED 1 |
| PE3 | Music LED 2 |
| PE4 | Music LED 3 |
| PE5 | IR Sensor input |
| PF4 | SW1 (left button) |
| PF0 | SW2 (right button) |

---

## FSM Design

The system uses a **Moore-style FSM** with 10 states. Outputs depend only on the current state, not directly on inputs.

| State | Description |
|---|---|
| OFF | All LEDs off |
| HOUSE | House LED on |
| SPEAKER | Waiting for IR detection, no LEDs |
| SPOTLIGHT | Spotlight LED on when speaker detected |
| MUSIC1–MUSIC6 | Repeating LED animation cycle |

### Button Input Encoding

| Input | SW1 | SW2 | Action |
|---|---|---|---|
| `00` | — | — | No change (drives music auto-advance) |
| `01` | — | pressed | Music mode / exit music |
| `10` | pressed | — | Speaker mode / exit speaker |
| `11` | pressed | pressed | Toggle OFF ↔ HOUSE |

### IR Sensor

The IR sensor (active low, PE5) fires on both edges via interrupt. When a speaker is detected in `SPEAKER` state, the system transitions to `SPOTLIGHT`. When the speaker leaves, it returns to `SPEAKER`.

---

## Implementation

- **FSM table** — defined as a struct array (`out`, `wait`, `next[4]`) for clean state lookup
- **Button interrupt** — falling-edge triggered on PF0/PF4 with debounce delay (~15ms)
- **IR interrupt** — both-edge triggered on PE5 with debounce delay (~3ms)
- **Music animation** — six states cycle automatically via `next[0]` (no-input path) with a `fresh` flag to prevent skipping the first state on entry

---

## Files

```
theater-lighting-fsm/
├── main.c       # Full implementation
└── README.md
```

> **Note:** Requires TivaWare driverlib (`driverlib/`, `inc/`) to compile. Tested on Code Composer Studio with a 16 MHz system clock.

---

## Authors

Vasanthavel Jeeva Kumararaja & Mathurtion Rajendrackumaar — Lehigh University, ECE132 Spring 2026
