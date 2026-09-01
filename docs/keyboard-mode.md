# USB MIDI / HID Keyboard mode

Rev.BはUSB Device/UFPだけを実装し、USB MIDIとUSB HID Boot Keyboardを一つの
Composite Deviceとして列挙します。mode切替でUSBを切断・再列挙しません。

```text
matrix → input_router ─┬→ MIDI transport (USB/TRS)
                       └→ HID report queue (USB)

USB MIDI RX ─┐
TRS MIDI IN ─┴→ MIDI parser → RGB state → renderer → RMT DMA
```

USB Host、DRP、Host VBUS source、role state machineはありません。単一USB-Cは
給電、MIDI、HID、native USB flashingを兼ねます。USB detach時は本体も無給電に
なるため、以前のself-powered VBUS monitorは不要です。CC状態とcurrent classは
TUSB320からI2Cで読みます。

HID keymapは `firmware/main/keyboard_keymap.c`、USB descriptorは
`firmware/main/midi_out_usb.c`、RGB受信は `midi_input.c` と `led_control.c` が正本です。
