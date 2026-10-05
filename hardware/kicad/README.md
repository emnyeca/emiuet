# KiCad sourceの位置づけ

- `Emiuet.kicad_sch`: 現行Rev.B schematic architecture draft（KiCad 10形式）
- `Emiuet_RevA.kicad_sch`: Rev.A historical schematic
- `Emiuet.kicad_pcb`: Rev.A historical PCB layout。今回のRev.B作業では未変更

Rev.B schematicとRev.A PCBはconnectivityが一致しません。historical boardに対してschematicからPCB updateを実行しないでください。Rev.B placement/routingは別のPCB redesignとして行います。

Rev.B draftではsingle USB-C UFP/power path、CC Rd＋comparatorによる電流広告検出、ESP32-S3 GPIO nets、3.3 V regulator候補、RGB level shifter、78-pixel serpentine data chain、bulk/local bypass方針、TRS MIDI IN/OUT、OLED connector、matrix/UI busesを確認できます。

これはproduction releaseではありません。reset/BOOT、CC検出、RGB bufferの電源接続とidle bias、OLED connectorを実回路化しました。regulatorとMIDI interfaceは未接続で、matrix/slider/button、input protection、各LEDのlocal bypass等は実回路化が必要です。component selection、footprints、接続設計、ERC確認と実機Validationを経て確定します。

OLED footprint `Eminuet Library:Waveshare_OLED_0.96in_C_7P` はRev.A PCBのBrd2（`EUB_SSD1306:9E6018AD`、B面・180°配置）から形状を変えずに書き出したものです。旧名・説明のAdafruit 1.3 inchは実物と異なります。

初期の実ファイル検証と再設計・EDA評価の記録は [Rev.B監査](../../docs/rev-b-audit.md) にあります（監査時点の状態）。

現在の進捗・再開手順は [ロードマップ](../../docs/rev-b-roadmap.md)、追加回路の根拠と機構条件は [回路ノート](../../docs/rev-b-circuit-notes.md) にあります。`python tools/verify_hardware.py` をリポジトリrootから実行すると、KiCad 10.0.3でERC/netlist/BOM/SVGと入力hashを `build/hardware/` に出力します。違反が残る現状では終了コード1が正常な検出結果です。出力BOMはレビュー用で、発注用ではありません。
