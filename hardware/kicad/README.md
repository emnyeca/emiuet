# KiCad sourceの位置づけ

- `Emiuet.kicad_sch`: 現行Rev.B schematic architecture draft（KiCad 10形式）
- `Emiuet_RevA.kicad_sch`: Rev.A historical schematic
- `Emiuet.kicad_pcb`: Rev.A historical PCB layout。今回のRev.B作業では未変更

Rev.B schematicとRev.A PCBはconnectivityが一致しません。historical boardに対してschematicからPCB updateを実行しないでください。Rev.B placement/routingは別のPCB redesignとして行います。

Rev.B draftではsingle USB-C UFP/power path、TUSB320、ESP32-S3 GPIO nets、3.3 V regulator候補、RGB level shifter、78-pixel serpentine data chain、bulk/local bypass方針、TRS MIDI IN/OUT、matrix/UI busesを確認できます。

これはproduction releaseではありません。input protection、regulator、MIDI interfaceの詳細、passive values、footprints、ERC cleanupはcomponent selectionと実機Validation後に確定します。
