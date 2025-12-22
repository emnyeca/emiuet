# Emiuet (EUB-04)
Fretboard-style MIDI Controller for Guitarists

## Overview

**Emiuet** is a fretboard-shaped MIDI controller designed specifically for guitarists.

Instead of adapting piano-style abstractions, Emiuet is built around
strings, positions, voicings, and controlled expressive gestures.
It is part of **Emnyeca’s Utility Build Series (EUB)** and represents a
focused, intentionally constrained musical instrument.

This repository contains hardware designs, firmware, and documentation
for the Emiuet prototype.

---

## Design Philosophy

Emiuet is intentionally **not** a general-purpose MIDI controller.

Its design is guided by the following principles:

- 🎸 **Guitar-first ergonomics**  
  A 6 × 13 key matrix mirrors fretboard thinking rather than keyboard layouts.

- 🎯 **Intentional constraints**  
  Certain features are deliberately limited to preserve musical clarity
  and predictable performance behavior.

- 🎛 **Expressive, but controlled**  
  Expressive gestures are supported only where they remain reliable and
  musically intentional.

- 🧠 **Readable by humans and AI collaborators**  
  Design decisions are explicit so that contributors and AI tools
  do not unintentionally “optimize away” core ideas.

---

## Hardware Summary

### Physical Specs

- Key matrix: **6 rows × 13 columns (78 keys)**
- Switches: Ambients Silent Choc 20g Linear
- Hybrid footprint (hot-swap capable)
- Target height: **≤ 30 mm**

### Enclosure / Exterior

- **Prototype**:
  - White / gold ENIG PCB is used as the top decorative surface
  - No acrylic top panel in the prototype stage
- **Future revisions**:
  - Acrylic, ENIG PCB, aluminum, or hybrid constructions are under consideration

The enclosure design is intentionally kept flexible during the prototype phase.

---

## MCU, Power, and I/O

- MCU: **ESP32-S3-MINI-1**
- Power:
  - Single-cell Li-ion battery (external)
  - Power-path charging architecture
- USB-C:
  - Port #1: Charging only
  - Port #2: USB-MIDI (DRP / OTG)

### MIDI Outputs

- USB-MIDI
- BLE-MIDI
- TRS MIDI (Type-A, 3.5 mm)

Note on TRS MIDI (firmware):
- The TRS MIDI OUT backend uses UART 31250 bps on `PIN_MIDI_OUT_TX` (UART0 TX).
- If you enable TRS UART output in Kconfig, move the ESP-IDF console off UART0
  (e.g., to USB Serial/JTAG), otherwise logs and MIDI will conflict.

**DIN MIDI OUT is intentionally not supported**  
and will not be added in this project.

### PIN and GPIO
Pin assignment is defined in docs/pinout-v3.md.

---

## UI & Control Logic

### Sliders

| Control | Function |
|------|--------|
| Slider 1 | Velocity |
| Slider 2 | CC#1 (Modulation) |
| Slider 3 | Pitch Bend |

### Pitch Bend Behavior (Intentional Constraint)

- Upward direction only
- Linear curve
- Fixed maximum range
- Automatic return to center when released

This behavior is **by design** and should not be generalized or made symmetrical.

### Performance Modes

Emiuet supports an optional MPE-style mode,
enabled via a dedicated physical toggle switch.

In this mode, each string (row) is assigned to an independent MIDI channel,
allowing string-specific pitch expression.

---

## Display

- OLED: SSD1315 0.96" (I²C, SSD1306-compatible)
- Graphics library: **u8g2 (required)**

The firmware assumes u8g2-based rendering.
Other display libraries are not supported.

Display usage is intentionally minimal and focused on performance feedback.

---

## Firmware

- Platform: ESP-IDF
- RTOS: FreeRTOS
- Arduino framework: **not used**

The firmware is structured around separated, non-blocking components
to ensure stable input scanning, MIDI generation, and UI updates.

This README intentionally avoids task-level or RTOS-internal details.
See the `docs/` directory for architectural notes.

---

## Repository Structure (Overview)

/hardware
/kicad
/bom
/pcba

/firmware
/components
/drivers
main.c

/docs
design-notes.md
decisions.md

---

## Development Notes for AI Agents (IMPORTANT)

This section exists explicitly to guide GitHub Copilot and other AI tools.

### Assumptions

- Target MCU is **ESP32-S3**
- ESP-IDF + FreeRTOS environment
- OLED rendering via **u8g2** is required and assumed by the firmware.
- Power state awareness (battery / USB / charging) is required

### Constraints (Do Not Violate)

- Do NOT add DIN MIDI support
- Do NOT add downward or symmetrical pitch bend
- Do NOT generalize this device into a piano-style controller
- Do NOT refactor hardware-dependent behavior without design context
- Do NOT assume that constraints are “temporary”

### Coding Preferences

- Explicit state handling
- Small, purpose-specific functions
- Constants defined with clear intent
- Comments explain *why*, not just *what*

Numeric hardware values are defined in schematics and source code,
not duplicated in this README.

---

## Project Context

Emiuet is developed by **Emnyeca**, a virtual jazz guitarist and founder of
**EMN Records**.

This project prioritizes musical intent and reliability over feature breadth.

---

## License

To be determined.  
An open-source-friendly license is planned after prototype validation.
