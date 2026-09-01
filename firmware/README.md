# Emiuet Rev.B firmware

ESP-IDF 5.3.4、ESP32-S3-MINI-1-N4R2向けです。

## Data paths

```text
matrix → input_router → USB/TRS MIDI TX or USB HID
USB MIDI RX / isolated TRS MIDI RX → midi_input → led_control
→ led_renderer → espressif/led_strip RMT DMA → SK6812 x78
TUSB320 → shared I2C → usb_power → renderer current limiter / OLED status
```

USBはbus-powered Composite Deviceです。self-powered VBUS GPIO、Host stack、role
negotiation、battery/charger stateはありません。

初期RGB inputはMIDI Note On/OffとCC123 All Notes Offです。global brightnessを含む
device-level LED control protocolは未確定です。CC7などのstandard musical CCを
device configurationへ転用せず、将来SysEx等の明示的なprotocolとして定義します。
SysEx拡張点はtransportの外側に残していますが、protocolはまだ固定しません。

## Build

ESP-IDF shellで次を実行します。

```text
idf.py -B build-revb -D SDKCONFIG=build-revb/sdkconfig \
  -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.rev-b.defaults" build
```

`espressif/led_strip`はRMT backendとESP32-S3 DMAを使用します。`sdkconfig.defaults`の
Default/1.5A LED budgetとbrightness ceilingはVAL-CORE-01/V3実測後に確定します。

Host-side logic testsは `tests/` にあります。ESP-IDF buildがUSB descriptor、I2C、
UART、RMTを含むintegration checkです。
