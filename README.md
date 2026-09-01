# Emiuet (EUB-04)

<p align="center">
  <img src="docs/assets/logo.png" width="420" alt="Emiuet logo">
</p>

Fretboard-style MIDI instrument for guitarists.

## What Emiuet is

**Emiuet** is a 6 × 13 fretboard MIDI controller built around the way guitarists
think: strings, fret positions, chord shapes, voicings, and controlled expressive
gestures. It is not intended to become a general-purpose pad controller or a
piano-style keyboard.

The thirteen positions preserve familiar guitar-derived voicings without forcing
octave shifts or omissions. Expressive controls are deliberately constrained so
that performance remains predictable: Pitch Bend is upward-only, and the optional
MPE-style mode separates strings by MIDI channel rather than attempting a fully
general per-note control system.

Emiuet is part of **Emnyeca's Utility Build Series (EUB)**. The project prioritizes
musical intent, reliability, and a clear physical mental model over feature breadth.

## Playing surface and controls

- 6 strings × 13 positions: 78 low-profile keys
- Three sliders: Velocity, CC#1 Modulation, and upward-only Pitch Bend
- Constrained stringwise MPE-style performance mode
- Minimal OLED feedback designed to avoid distracting from performance
- USB MIDI, Type-A TRS MIDI OUT/IN, and BLE capability
- Auxiliary USB HID Keyboard mode (`TYPE`)
- One RGB LED under each key for fretboard visualization

`TYPE` is a desk-convenience feature for short text, navigation, and shortcuts.
It reuses the physical matrix but does not change Emiuet's identity as a musical
instrument. See [USB HID Keyboard Mode](docs/keyboard-mode.md) for the complete
layout and Fn layer.

## Rev.B hardware summary

Rev.B simplifies Emiuet into a USB-powered Device/UFP:

- ESP32-S3-MINI-1 using native USB
- One USB-C port for 5 V power, USB MIDI, USB HID, and firmware flashing
- Persistent USB MIDI + HID Keyboard composite device
- TUSB320 used only for USB-C attach, orientation, and Source current detection
- No USB Host, DRP, Host VBUS sourcing, internal battery, charger, or PowerPath
- SK6812 MINI-E ×78 on one data chain with a 3.3 V-to-5 V buffer
- Firmware-limited LED brightness/current based on conservative USB power budgets

Mobile use is supported with an external USB power bank. A 3 A Source advertisement
does not raise Emiuet's internal design ceiling above approximately 5 V / 1.5 A.

## MIDI and RGB behavior

Performance logic remains independent of individual transports. MIDI generation
must not block on USB, TRS, or BLE output. Incoming USB or TRS Note On/Off messages
can drive fretboard visualization through a transport-independent LED state layer.

The external protocol for device-level LED settings such as global brightness is
not defined yet. Standard musical Control Change messages are not repurposed for
device configuration; a future explicit protocol may use SysEx.

## Hardware and firmware sources

- Current Rev.B schematic draft: `hardware/kicad/Emiuet.kicad_sch`
- Historical Rev.A schematic: `hardware/kicad/Emiuet_RevA.kicad_sch`
- Rev.B GPIO allocation: `docs/pinout-v3.md`
- Current design decisions: `docs/decisions.md`
- Firmware: ESP-IDF 5.3.4 + FreeRTOS; OLED rendering uses u8g2

The checked-in PCB layout is historical Rev.A data and was not updated with the
Rev.B schematic. Rev.B placement and routing require a separate PCB redesign.

## Project status

The Rev.B schematic is an architecture draft, not a fabrication-ready release.
Protection parts, regulator and MIDI interface selections, passive values, ERC
cleanup, PCB layout, and physical validation remain open engineering work.

## License

To be determined. An open-source-friendly license is planned after prototype
validation.
