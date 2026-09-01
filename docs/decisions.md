# Rev.B design decisions

## Product boundary

Rev.Bは単一USB-Cから給電されるUSB Device/UFPです。USB Host、DRP、OTG role
switching、Host VBUS source、内蔵電池、充電は製品境界外です。USB MIDIとUSB HID
KeyboardはESP32-S3 native USB上の一つのComposite Deviceとして常時列挙します。

## Power and Type-C

VBUSはresettable fuseまたはeFuse/load switch、TVS、必要なEMI conditioningを通って
5V railになります。5V railはRGBへ直接配電し、3.3V regulatorを介してMCU/OLED/
logicへ給電します。hardware ceilingは5V/1.5Aです。

TUSB320は削除しません。`PORT`をGND、`ADDR`をGND (`0x60`)、`EN_N`をGNDとし、
CC1/CC2のRd、attach、orientation、Source current advertisementを担当します。
`VBUS_DET`はTI指定の900kΩを介してVBUSへ接続します。I2C register `0x08[5:4]`
でDefault/1.5A/3A、`0x09[7:6]`でAttached.SNK、`0x09[5]`で向きを読みます。
Sourceが接続中にRpを変更する場合に備え、firmwareはTIの指示どおり定期的に
I2C soft resetを行います。

CCのDefault表示だけではUSB 2.0の500mAとUSB 3.xの900mAを区別できません。
したがってDefault予算は保守的にし、1.5A/3A表示は同じ1.5A ceilingへ丸めます。
現在のLED-only候補値200mA/1000mAとbrightness上限96/255は検証用初期値で、
量産確定値ではありません。

## RGB

SK6812 MINI-Eを6×13、合計78個搭載します。電源は各pixelへ並列、dataだけを
single daisy chainにします。各pixelに100nF local bypass、5V入口付近に
470–1000µF級bulk capacitorの実装余地を設けます。値と実装個数はV2/V3で確定します。

ESP32-S3 `GPIO38`から5V給電のSN74AHCT1G125へ入り、33–100Ω候補のseries resistorを
経て最初のDINへ接続します。初期値は74AHCT1G125、68Ω、100nF/pixel、680µFです。
これらは検証対象であり、BOM lock前に波形、EMI、突入、温度を確認します。

firmwareはEspressif `led_strip`のRMT backendとDMAを使います。bit-bangは使いません。
物理serpentine chainと6×13論理座標はmapping functionで分離します。

## MIDI

TRS MIDI OUTはType-Aを維持します。TRS MIDI INも追加します。UART1を
`GPIO43(TX)/GPIO44(RX)`へ割り当て、INはMIDI Associationの電気仕様に従う
optocoupler絶縁回路とします。受信jackのsignal/shieldにDC ground pathを作りません。
正確なoptocoupler、TRS jack、resistor値は部品入手性を含めVAL-CORE-01で確定します。

## Removed Rev.A circuits

- charging-only USB-C、Li-ion connector、NTC、BQ24074、charge configuration
- battery protection dependency、PowerPath、BAT_VSENSE、charge/PG GPIO
- TPS61023 battery-to-5V boost
- LM66100 Host VBUS source path
- second USB-C、Host/DRP/role-switch circuitry

TPS61023とLM66100にはRev.Bで別用途がないため削除します。Rev.A historical schematicは
保持し、Rev.B PCBは新schematicから作り直します。

## Sources used for electrical decisions

- TI TUSB320 datasheet: https://www.ti.com/lit/ds/symlink/tusb320.pdf
- Espressif ESP32-S3-MINI-1 datasheet: https://documentation.espressif.com/esp32-s3-mini-1_mini-1u_datasheet_en.html
- Espressif led_strip: https://components.espressif.com/components/espressif/led_strip
- MIDI 1.0 Electrical Specification Update: https://www.midi.org/wp-content/uploads/wpforo/default_attachments/1709416667-ca33-MIDI-10-Electrical-Specification-Update.pdf
- TI SN74AHCT1G125 datasheet: https://www.ti.com/lit/ds/symlink/sn74ahct1g125.pdf
