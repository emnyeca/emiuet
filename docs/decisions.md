# Emiuet 現行設計判断

この文書はEmiuetの現在有効なproduct decisionsの正本です。実装済み範囲や作業履歴ではなく、変更時に守るべき目的、主従関係、非自明な制約を記録します。撤回済みのRev.A/旧Rev.B判断は `history/rev-a-and-superseded-decisions.md` に分離します。

## 1. Product identity: guitar-first instrument

Emiuetはguitarists向けのfretboard MIDI instrumentです。general-purpose MIDI controller、piano-style keyboard、商用MIDI keyboardとのfeature parityを目的にしません。

strings、fret positions、chord shapes、voicings、guitar由来のmuscle memoryを中心に設計します。USB、RGB、TYPE modeなどのsubsystemはこの演奏体験を補助する手段であり、product identityより上位ではありません。

## 2. 6 × 13 playing surface

key matrixは6 rows × 13 columnsに固定します。13 positionsは数値上の対称性ではなく、E-root/A-rootを含むguitar由来のvoicingをoctave shiftや強制的な省略なしで扱うために選択しています。

PCB幅とrouting complexityが増えても、guitaristsのharmonic thinkingを維持することを優先します。

## 3. Pitch Bend

Pitch Bendはstring bendingに対応する上方向のみ、linear curve、固定maximum range、自動center returnとします。下方向または対称的なbendへ一般化しません。

これは実装上の暫定制限ではなく、予測可能な演奏表現を守る意図的な制約です。

## 4. Constrained MPE

任意のStringwise Bend modeでは、各stringを独立MIDI channelへ割り当てます。Pitch Bendは直近に演奏したstringを対象とし、bend中は対象を固定します。full generalized MPEではなく、guitar-like mental modelを保つ限定的なchannel separationを採用します。

表現力を増やす場合も、channel managementの複雑さが演奏の予測可能性を損なわないことを判断基準にします。

## 5. Minimal OLED

OLEDはperformance feedbackに限定します。menu-driven parameter editorや高密度情報表示を本体UIの中心にしません。複雑な設定UIが必要な場合は外部device側を優先し、演奏中の視覚的負担とfirmware責務を増やしません。

## 6. MIDI transport philosophy

USB、TRS、BLEは独立したtransportです。musical logicはtransport I/Oを待たず、dedicated sender taskへ0-waitでenqueueします。通常のrealtime priorityはTRS > USB = BLEで、同時出力を許可し、自動fallbackは前提にしません。

Pitch BendとCC#1のようなcontinuous controlはchannel単位でcoalesceでき、Note On/Offなどのdiscrete eventは順序を維持します。BLE transportがstubの間はroutingで有効化しません。

TRS MIDIは薄型筐体に適したType-Aを使用し、5-pin DIN MIDIは採用しません。Rev.BではType-A TRS MIDI OUTを維持し、isolated Type-A TRS MIDI INも追加します。

## 7. Auxiliary TYPE mode

USB HID Keyboard Mode (`TYPE`) はdesk上での短いtext entry、search、navigation、shortcutを補助する機能です。EmiuetをPC keyboardとして再定義せず、通常keyboardの速度やergonomicsを目標にしません。

MIDIとHID Keyboardは一つのpersistent composite USB Deviceとして列挙し、mode switchingでUSBを再enumerateしません。4 corner keysを2秒保持してmodeを切り替え、transition時はtracked note/keyとpending stateを解放します。actual layoutは `keyboard-mode.md` をユーザー向け正本とします。

## 8. Rev.B USB and power

Rev.Bはsingle USB-C、USB Device/UFP onlyです。一つのportで5 V power、USB MIDI、USB HID、firmware flashingを担います。USB Host、DRP、OTG role switching、Host VBUS source、runtime role switching、internal battery、charger、PowerPathは採用しません。

power architectureは次の通りです。

```text
USB-C VBUS
→ protection / input conditioning
→ 5 V rail → RGB LEDs
             └→ 3.3 V regulator → ESP32-S3 / OLED / logic
```

hardware ceilingは約5 V / 1.5 Aです。mobile useはexternal USB power bankを使用します。

## 9. TUSB320

TUSB320は削除せず、fixed UFPのCC/current detectorとして限定利用します。`PORT=L`、`ADDR=L` (`0x60`)、`EN_N=L`とし、CC1/CC2 attach、orientation、Default/1.5 A/3 A Source advertisementをI2Cで取得します。USB Host、DRP、role control、VBUS source controlには使いません。

Default advertisementではUSB 2.0 500 mAとUSB 3.x 900 mAをCCだけから区別できないため、firmwareは安全側のLED budgetを選びます。1.5 A/3 Aは同じ1.5 A hardware ceilingへ丸めます。接続中のRp変更を反映するため、firmwareはdatasheetに従ってperiodic I2C soft resetを行います。

## 10. RGB fretboard visualization

SK6812 MINI-Eを全78 keysへ搭載し、dataはsingle serpentine daisy chain、5 V/GNDはparallel distributionとします。physical chain orderと6 × 13 logical fret/string coordinatesをfirmware mappingで分離します。

ESP32-S3 GPIO38から5 V AHCT bufferとseries resistorを介してDINへ接続します。各pixelのlocal bypassと5 V bulk capacitanceを設けますが、部品値はValidation前の確定仕様としません。

rendererはEspressif `led_strip`、RMT backend、ESP32-S3 DMAを使用し、bit-bangを採用しません。USB/TRS MIDI RX → parser → LED state → rendererを分離します。

Note On/Offによるfretboard visualizationは初期機能として採用します。global brightnessなどのdevice settingを標準musical CCへ割り当てません。外部LED control protocolは未確定で、将来SysEx等の明示的なprotocolとして検討します。

## 11. Rev.Bから撤回した回路

- charging-only second USB-C
- Li-ion cell/connector、NTC、BQ24074、charge configuration
- battery protection dependency、PowerPath、BAT_VSENSE、charge/PG GPIO
- TPS61023 battery-to-5 V boost
- LM66100 Host VBUS source path
- Host/DRP/role-switch circuitry

TPS61023とLM66100にはRev.Bで別用途がないため採用しません。Rev.A manufacturing dataはhistoryとして保持し、Rev.B PCBはcurrent schematicから別工程で再設計します。
