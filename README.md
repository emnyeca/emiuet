# Emiuet

Emiuet Rev.Bは、78キーの演奏面を持つUSB-powered MIDI/HID controllerです。
製品仕様の正本はこのリポジトリです。Validationリポジトリは試験方法と結果だけを管理します。

## Rev.B current architecture

```text
single USB-C Device/UFP
├─ VBUS → input protection/switch → +5V_LED → SK6812 MINI-E x78
│                                  └→ 3.3V regulator → ESP32-S3/OLED/logic
├─ CC1/CC2 → TUSB320 (fixed UFP, I2C status)
└─ D-/D+ → protection → ESP32-S3 native USB
                         └─ USB MIDI + HID Keyboard composite device

USB/TRS MIDI RX → MIDI parser → LED state → logical/physical map
                → current-limited renderer → RMT DMA → RGB chain
```

Rev.Bの外部電源は単一USB-Cの5Vだけです。内蔵電池、充電器、PowerPath、
5V boost、USB Host、DRP、Host VBUS sourceは搭載しません。モバイル用途では
外付けUSB power bankを使います。

TUSB320は残しますが、`PORT=L`の固定UFPです。CC attach、orientation、Sourceの
Default/1.5A/3A advertisementだけをI2Cで読みます。3Aを検出しても製品上限は
5V/1.5Aのままです。Default時のLED予算は安全側に倒し、最終値は実機検証で決めます。

## Main functions

- ESP32-S3-MINI-1-N4R2、native USB D-/D+ (`GPIO19/20`)
- USB MIDI + USB HID Keyboard composite Device
- 6×13 key matrix、3 analog sliders、3 buttons、OLED
- Type-A TRS MIDI OUTと、絶縁Type-A TRS MIDI IN
- SK6812 MINI-E ×78、単一chain、RMT DMA
- BLE capabilityは保持するがRev.B hardware acceptanceの必須条件ではない

## Hardware files

- `hardware/kicad/Emiuet.kicad_sch`: Rev.B current schematic draft
- `hardware/kicad/Emiuet_RevA.kicad_sch`: Rev.A historical schematic
- `hardware/kicad/Emiuet.kicad_pcb`: Rev.A historical layout。今回のRev.B作業では未変更
- `docs/pinout-v3.md`: Rev.B GPIO source of truth
- `docs/decisions.md`: current design decisions and validation boundaries

Rev.B schematicは設計レビュー用の初期たたき台です。既存PCBとのconnectivityは
一致しません。Rev.B PCB placement/routing/outlineは次工程で全面更新します。

## Firmware

ESP-IDF 5.3.4を使用します。USB受信、LED状態、renderer、RMT transportを分離し、
複雑なSysEx protocolはまだ定義していません。初期受信はNote On/Off、CC7による
global brightness、CC123 All Notes Offを扱います。

詳細は `firmware/README.md` と `docs/keyboard-mode.md` を参照してください。

## Historical material

Rev.Aおよび撤回済みRev.B案は `docs/history/` と `hardware/kicad/Emiuet_RevA.kicad_sch`
に保存しています。そこにあるbattery、dual USB、Host/DRPの記述は現行仕様ではありません。
