# Emiuet firmware

Emiuet firmware targets ESP32-S3 with ESP-IDF 5.3.4. Arduino is not used.

The matrix scanner produces physical press/release events. `input_router`
routes those events to either the existing MIDI path or the USB HID keyboard
path according to the explicit input mode. USB-MIDI and HID are exposed
together as one composite USB device, so switching MIDI/TYPE does not require
USB disconnect or re-enumeration.

## Build

Use an ESP-IDF 5.3.4 shell:

```text
idf.py build
```

The managed component lock selects `esp_tinyusb` 2.0.1~1 and TinyUSB
0.19.0~2. Both `CONFIG_TINYUSB_MIDI_COUNT=1` and
`CONFIG_TINYUSB_HID_COUNT=1` are required.

## Input-mode structure

```text
matrix_scan -> input_router -> MIDI output
                           -> keyboard_input -> USB HID queue
```

Mode changes and USB detach clear all tracked MIDI notes, send MIDI All Notes
Off/pitch-bend center, discard stale USB-MIDI queue contents, and clear the HID
report queue. The pitch-bend slider continues to be sampled in TYPE mode but
does not emit MIDI.

The editable keyboard mapping is centralized in `main/keyboard_keymap.c`.
See [`../docs/keyboard-mode.md`](../docs/keyboard-mode.md) for the user-facing
layout and USB-role findings.

## Host-side logic tests

The keymap and keyboard report-state tests are in `tests/`. They are small,
libc-free C programs so they can be built with the bundled host clang even on
a machine without a separate desktop C toolchain. The firmware build remains
the integration check for the ESP-IDF/TinyUSB descriptor and task code.
