# Emiuet – Design Notes

This document records design rationale, constraints, and implementation intent
that are intentionally omitted from the README.

It exists to preserve musical and engineering intent across revisions,
contributors, and AI-assisted development.

This is not a user manual.

---

## 1. Overall System Intent

Emiuet is designed as a performance instrument, not a development board.

Design decisions prioritize:
- Stable behavior during live performance
- Predictable response over feature completeness
- Musical intent over configurability

Some constraints may appear arbitrary from a purely technical perspective.
They are not.

---

## 2. Power System Design Rationale

### 2.1 Why a Power-Path Architecture

Emiuet assumes that:
- External USB power is not always clean or reliable
- The instrument may be used while charging
- Power transitions must not affect musical output

For these reasons, a power-path architecture is used so that
system operation and battery charging are decoupled.

### 2.2 Battery-First Thinking

The system is designed to operate as a battery-powered instrument first.

USB power is treated as:
- A charging source
- A temporary external supply

It is not assumed to be a stable system reference.

---

## 3. Startup Stability and Boot Safety

Stable startup behavior is considered a design requirement, not an implementation detail.

Certain MCU pins affect boot configuration and are intentionally excluded
from dynamic input or scanning roles.

The MCU's EN (Enable) pin requires an external RC delay circuit
to guarantee stable power-on sequencing.
This is a critical electrical requirement and is not optional.

Startup instability is treated as a hardware design issue rather than
something to be compensated for in software.

---

## 4. Input System and Noise Considerations

### 4.1 Key Matrix Philosophy

The key matrix is optimized for:
- Consistent scanning latency
- Predictable debounce behavior
- Minimal interaction with boot-sensitive pins

Matrix complexity is accepted in exchange for input reliability.

### 4.2 Slider Inputs and Noise Reality

Analog inputs are assumed to be noisy.

Both hardware-level and firmware-level mitigation are applied,
with the goal of achieving repeatable musical gestures rather than raw resolution.

---

## 5. Pitch Bend

### 5.1 Pitch Bend as a Musical Constraint

Pitch bend behavior in Emiuet is intentionally constrained.

- Direction is limited
- Range is fixed
- Behavior is predictable and repeatable

This limitation is not technical.
It is musical.

The design favors controlled expressive gestures similar to guitar bending,
rather than symmetrical or configurable pitch modulation.

Pitch bend stability relies on reliable return-to-center behavior.
This includes explicit reset logic and noise mitigation
to ensure repeatable expressive gestures.

### 5.2 MPE Mode as a Guitar-Oriented Constraint

Emiuet includes an optional MPE-style mode,
designed to support string-specific expression.

This mode is intentionally constrained.

While each string operates on its own MIDI channel,
pitch bend behavior remains direction-limited and range-fixed.

Pitch bend is applied only to the most recently active string,
reflecting the physical reality of guitar bending,
where expression is localized rather than globally distributed.

This is not a full implementation of generalized MPE.
It is a guitar-oriented reinterpretation of MPE concepts.

## 5.3. MPE Mode and Localized Pitch Expression

In MPE mode, each string (row) operates on its own MIDI channel.

Pitch bend is intentionally applied only to the most recently active string.
This behavior is a core part of the performance logic, not a side effect.

It reflects the physical reality of guitar playing,
where pitch expression is localized to a single string
rather than distributed across all sounding notes.

This rule defines how expressive gestures are interpreted by the instrument
and must be preserved to maintain predictable, guitar-like behavior.

---

## 6. Display

### 6.1 Display Strategy and OLED Usage

The OLED display is not intended to be a primary UI.

Its role is:
- Performance feedback
- State confirmation
- Minimal visual distraction

Display complexity is intentionally capped.
If information is not required during performance, it is not displayed.

The firmware assumes a specific graphics library to ensure consistent
timing and rendering behavior.

## 6.2 Display Constraints and Rendering Assumptions

The OLED display is intentionally limited both by design policy
and by technical constraints.

Rendering is based on the u8g2 graphics library,
which is assumed by the firmware.

Given rendering performance and memory constraints,
the display is limited to:
- Key press visualization
- Octave shift condition
- Basic system state confirmation (e.g., power or battery state)

The display is not intended to support dense UI elements or menus,
as this would interfere with performance timing and visual focus.

---

## 7. Firmware Structure (High-Level)

The firmware is structured so that:
- Input scanning cannot be blocked by rendering or communication
- MIDI output timing remains consistent under load
- UI updates are isolated from musical logic

Real-time behavior is prioritized over architectural elegance.

### 7.x MIDI Output (Non-Blocking + Coalescing)

As a musical instrument, Emiuet prioritizes stable, low-latency MIDI output.
However, the input/musical logic (matrix scan / slider / MPE) must never block on I/O.

Implementation intent:
- Each transport (TRS UART / USB / BLE) has a dedicated sender task and a discrete-event queue.
- `midi_out_send_*()` must return immediately; it only enqueues.
- Continuous controls are coalesced to prevent queue saturation:
	- Pitch Bend: latest value wins (per MIDI channel)
	- CC#1 (Modulation): latest value wins (per MIDI channel)
- Output realtime priority among transports is TRS > USB = BLE.
	Simultaneous output is allowed.

---

## 7.1 USB-MIDI Bring-up Note (DevKit vs Prototype)

Some ESP32-S3 DevKits expose *two different* USB paths:

- USB-Serial/JTAG (debug/programming) port
- Native USB OTG port (D+/D- on GPIO20/GPIO19)

If the cable is connected to the USB-Serial/JTAG port, TinyUSB (USB OTG) can be fully initialized in firmware yet **Windows will not enumerate any MIDI device**, and `tud_mount_cb()` / `tud_mounted()` will never trigger.

TODO (when the Emiuet prototype PCB arrives):
- Verify the cable is connected to the native USB OTG port wired to GPIO20/GPIO19 (see pinout-v3).
- If Windows still enumerates only Serial/JTAG, temporarily disable USB-Serial/JTAG in `menuconfig` so TinyUSB can own D+/D-.
- Confirm host enumeration first (Device Manager / MIDI device listing) before debugging MIDI message flow.

---

## 8. Things That Look Flexible but Are Not

The following aspects may appear configurable but are intentionally fixed:

- Pitch bend direction and range
- Absence of DIN MIDI output
- TRS MIDI (Type-A) output requirements
- Display information density
- Assumptions about USB power quality

TRS MIDI (Type-A) output must use a dedicated 5V current loop circuit,
compliant with MIDI specifications, and must not be driven directly
by 3.3V logic.

These constraints are considered part of the instrument’s identity.

---

## 9. Where Changes Are Acceptable

The following areas are considered safe for experimentation:

- Visual design and enclosure materials
- Non-performance-critical UI elements
- Firmware refactoring that preserves observable behavior

Any change affecting musical response or power stability
should be evaluated with caution.

---

End of design notes.
