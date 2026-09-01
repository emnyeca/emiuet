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

## USB role and Rev.B boundary

Rev.B formally uses a fixed **USB Device / UFP** role. USB Host, DRP/OTG role
switching, Host VBUS sourcing, and Device/Host runtime transitions are outside
the initial product scope by design, not merely unimplemented. MIDI/TYPE mode
switching therefore never changes the USB configuration.

The checked-in KiCad schematic and manufacturing outputs remain the historical
Rev.A design. Rev.A U5 is a TUSB320 configured as DRP, and internal +5V reaches
USB-C #2 VBUS through LM66100. Rev.B replaces those role/source circuits with
fixed CC pull-downs and protected VBUS presence sensing. See the fixed-role
decision and hardware delta in [`decisions.md`](decisions.md#10-revb-fixed-usb-device-role).

Emiuet is self-powered from its battery/system rails, so the Rev.B VBUS monitor
must be connected to the ESP32-S3 and enabled in `esp_tinyusb`. Physical detach
detection and electrical behavior remain hardware validation items until that
Rev.B circuit is built.

The software constraints are consistent with Espressif's ESP32-S3 USB Device
and self-powered Device documentation. The Rev.A historical findings use TI's
TUSB320 and LM66100 datasheets.

Official references:

- [ESP-IDF 5.3.1 ESP32-S3 USB Device Stack](https://docs.espressif.com/projects/esp-idf/en/v5.3.1/esp32s3/api-reference/peripherals/usb_device.html)
- [esp_tinyusb self-powered Device configuration](https://components.espressif.com/components/espressif/esp_tinyusb/versions/2.0.1/readme)
- [TI TUSB320 datasheet](https://www.ti.com/lit/ds/symlink/tusb320.pdf)
- [TI LM66100 datasheet](https://www.ti.com/lit/ds/symlink/lm66100.pdf)
