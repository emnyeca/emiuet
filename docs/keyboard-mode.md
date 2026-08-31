# USB HID Keyboard Mode

## Positioning

USB HID Keyboard Mode (`TYPE`) is an auxiliary input feature that makes it
easier to keep Emiuet on a desk without always placing a second keyboard next
to it. It covers short text entry, search, file operations, shortcuts, and
auxiliary coding input. Emiuet remains first and foremost a fretboard MIDI
instrument; its 17 mm pitch, ortholinear 6 × 13 grid, and mostly 1U keys are not
intended to match a conventional PC keyboard's speed or ergonomics.

## Modes and switching

- `MIDI`: matrix presses generate MIDI notes; no HID keyboard reports are generated.
- `TYPE`: matrix presses generate USB HID keyboard reports; matrix presses and sliders do not generate MIDI.

Hold the four physical corner keys `(row 1, column 1)`, `(row 1, column 13)`,
`(row 6, column 1)`, and `(row 6, column 13)` together for 2 seconds to switch
modes. The four corner positions are reserved in the keyboard layout. In MIDI
mode they retain their musical note behavior until the switch occurs.

The chord requires four widely separated keys and a long hold, making an
accidental switch during normal performance unlikely. A mode change does not
restart USB. On every transition the firmware releases all tracked notes or
the entire HID report and resets pitch bend to center. A USB detach also
discards pending USB output so stale events are not replayed after reconnect.

## Base layer

Rows below are shown left to right as physical columns 1–13. `MODE` means a
corner reserved for the mode chord.

| Row | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | MODE | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 0 | - | MODE |
| 2 | Tab | Q | W | E | R | T | Y | U | I | O | P | [ | ] |
| 3 | Caps | A | S | D | F | G | H | J | K | L | ; | ' | Enter |
| 4 | LShift | Z | X | C | V | B | N | M | , | . | / | RShift | Backslash |
| 5 | LCtrl | LGUI | LAlt | Fn | Space | Space | Space | Space | Space | RAlt | Left | Down | Right |
| 6 | MODE | Esc | Grave | = | Backspace | Delete | Insert | Menu | Home | End | PageUp | Up | MODE |

The HID usages describe US-keyboard positions. The host operating system's
active keyboard layout determines the character actually produced, so symbol
legends can differ under non-US layouts.

## Fn layer

Fn is momentary. It changes the active mapping while held.

| Row | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | MODE | F1 | F2 | F3 | F4 | F5 | F6 | F7 | F8 | F9 | F10 | F11 | MODE |
| 2 | Tab | Home | Up | End | — | — | — | — | — | — | PrintScreen | ScrollLock | Pause |
| 3 | Caps | Left | Down | Right | — | — | — | — | — | — | — | — | Enter |
| 4 | LShift | — | — | — | — | — | — | — | — | — | — | RShift | — |
| 5 | LCtrl | LGUI | LAlt | Fn | Space | Space | Space | Space | Space | RAlt | Home | PageDown | End |
| 6 | MODE | F12 | Esc | = | Backspace | Delete | Insert | Menu | Home | End | PageUp | Up | MODE |

The boot-keyboard report supports six simultaneous non-modifier usages plus all
modifier bits. This covers normal shortcuts such as Shift+letter, Ctrl+C,
Ctrl+V, and Ctrl+Z. The diode-equipped matrix itself continues to scan all
physical keys; only the standard USB boot-keyboard report is limited to 6KRO.

## OLED

The top status area shows `MIDI OCT:n` in MIDI mode. TYPE mode shows
`TYPE L0 C:OFF` or the current layer and host-reported Caps Lock LED state.

## USB implementation

The firmware is locked to ESP-IDF 5.3.4, `esp_tinyusb` 2.0.1~1, and TinyUSB
0.19.0~2. It installs one persistent full-speed composite USB device:

```text
USB configuration
├─ interface 0: MIDI Audio Control
├─ interface 1: MIDI Streaming (OUT 0x01, IN 0x81)
└─ interface 2: HID boot keyboard (IN 0x82)
```

This uses three non-control endpoints and is within ESP32-S3's endpoint budget.
HID reports use a dedicated non-blocking queue. USB detach discards both MIDI
and HID pending data so events performed while disconnected are never replayed
after reconnection.

The prototype descriptor retains the repository's existing Espressif VID
`0x303A` and PID `0x4005`. A production or publicly distributed device needs
an assigned or otherwise authorized USB VID/PID; this is a release/compliance
decision, not a PCB requirement.

## USB role and Rev.B findings

The composite Device feature is firmware-only and requires no PCB change.
Host/Device role switching is a separate, unresolved hardware/firmware concern:

- Rev.A U5 is a TUSB320. `PORT` is intentionally left unconnected, selecting DRP; `ADDR` is pulled low, selecting I²C address `0x60`.
- SDA/SCL share the MCU's I²C bus, but TUSB320 `ID` and `INT_N` are only pulled up and are not routed to an ESP32-S3 GPIO. No current firmware reads TUSB320 registers or manages USB roles.
- The ESP32-S3 has one USB-OTG controller and cannot operate that controller as Host and Device simultaneously. Software can tear down one stack and initialize the other, which necessarily disconnects and re-enumerates.
- Rev.A's LM66100 VBUS2 path has `CE` pulled high rather than controlled by the MCU or TUSB320. A safe, negotiated Host-mode VBUS source is therefore not established by the present firmware design.
- Emiuet can run from its battery while USB is absent. Espressif's self-powered Device guidance therefore makes physical detach detection and VBUS sensing a bench-validation item; Rev.A does not route a dedicated VBUS-presence signal to an ESP32-S3 GPIO. The implemented TinyUSB detach callback is safe at the software level, but electrical/compliance behavior must be confirmed on hardware.

Consequently, this feature formally supports composite **USB Device** operation.
Device → Host → Device switching has not been claimed or implemented. If USB
Host becomes a Rev.B requirement, the role-status signal, controllable and
current-limited VBUS sourcing, VBUS discharge/backfeed behavior, and a complete
stack teardown/restart state machine must be reviewed together. Those changes
must not be inferred as necessary for MIDI + HID composite Device mode.

The hardware findings above come from the checked-in Rev.A KiCad PCB/schematic.
The software constraints are consistent with Espressif's ESP32-S3 USB Device,
USB Host, and USB FAQ documentation, and TI's TUSB320 datasheet.

Official references:

- [ESP-IDF 5.3.1 ESP32-S3 USB Device Stack](https://docs.espressif.com/projects/esp-idf/en/v5.3.1/esp32s3/api-reference/peripherals/usb_device.html)
- [ESP32-S3 USB Host](https://docs.espressif.com/projects/esp-idf/en/release-v5.3/esp32s3/api-reference/peripherals/usb_host.html)
- [Espressif USB FAQ](https://docs.espressif.com/projects/esp-faq/en/latest/software-framework/peripherals/usb.html)
- [TI TUSB320 datasheet](https://www.ti.com/lit/ds/symlink/tusb320.pdf)
