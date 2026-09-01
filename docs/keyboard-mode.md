# USB HID Keyboard Mode

## Positioning

USB HID Keyboard Mode (`TYPE`) is an auxiliary input feature for short text
entry, search, file operations, navigation, shortcuts, and occasional coding
input. Emiuet remains first and foremost a fretboard MIDI instrument. Its
6 × 13 ortholinear playing surface is not intended to reproduce the speed or
ergonomics of a conventional PC keyboard.

## Modes and switching

- `MIDI`: matrix presses generate MIDI notes; no HID keyboard reports are generated.
- `TYPE`: matrix presses generate USB HID keyboard reports; matrix presses and sliders do not generate MIDI.

Hold all four physical corner keys—row 1 columns 1 and 13, and row 6 columns 1
and 13—for two seconds to switch modes. The widely separated, long-hold chord
reduces accidental switching during performance. A mode change does not restart
or re-enumerate USB.

Every transition releases tracked MIDI notes or the complete HID report and
returns Pitch Bend to center. Because Rev.B is powered by its single USB-C port,
physical detach also powers the unit down rather than leaving a self-powered USB
session active.

## Base layer

Rows are shown left to right as physical columns 1–13. `MODE` marks a corner
reserved for the switching chord.

| Row | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | MODE | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 0 | - | MODE |
| 2 | Tab | Q | W | E | R | T | Y | U | I | O | P | [ | ] |
| 3 | Caps | A | S | D | F | G | H | J | K | L | ; | ' | Enter |
| 4 | LShift | Z | X | C | V | B | N | M | , | . | / | RShift | Backslash |
| 5 | LCtrl | LGUI | LAlt | Fn | Space | Space | Space | Space | Space | RAlt | Left | Down | Right |
| 6 | MODE | Esc | Grave | = | Backspace | Delete | Insert | Menu | Home | End | PageUp | Up | MODE |

The usages follow US keyboard positions. The active host keyboard layout
determines the character produced, so symbol legends may differ on other layouts.

## Fn layer

Fn is momentary and changes the active mapping only while held.

| Row | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | MODE | F1 | F2 | F3 | F4 | F5 | F6 | F7 | F8 | F9 | F10 | F11 | MODE |
| 2 | Tab | Home | Up | End | — | — | — | — | — | — | PrintScreen | ScrollLock | Pause |
| 3 | Caps | Left | Down | Right | — | — | — | — | — | — | — | — | Enter |
| 4 | LShift | — | — | — | — | — | — | — | — | — | — | RShift | — |
| 5 | LCtrl | LGUI | LAlt | Fn | Space | Space | Space | Space | Space | RAlt | Home | PageDown | End |
| 6 | MODE | F12 | Esc | = | Backspace | Delete | Insert | Menu | Home | End | PageUp | Up | MODE |

The USB boot-keyboard report supports six simultaneous non-modifier usages plus
all modifier bits. This covers shortcuts such as Shift+letter, Ctrl+C, Ctrl+V,
and Ctrl+Z. The diode-equipped matrix continues to scan every physical key; only
the HID boot-keyboard report is limited to 6KRO.

## OLED feedback

The top status area shows `MIDI OCT:n` in MIDI mode. TYPE mode shows `TYPE L0
C:OFF`, or the current layer and host-reported Caps Lock LED state.

## USB boundary

Rev.B exposes one persistent full-speed USB composite Device with MIDI and HID
Keyboard interfaces. MIDI/TYPE switching changes only the internal event route.
USB Host, DRP/OTG role switching, Host VBUS sourcing, and runtime role changes
are outside the product scope.

The single USB-C port also powers the instrument and supports native USB firmware
flashing. TUSB320 is retained only to observe UFP attach, cable orientation, and
Source current advertisement; it does not control USB roles.
