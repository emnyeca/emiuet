# Rev.B 回路設計ノート

更新: 2026-10-05。内部レビュー用。製品判断は [decisions.md](decisions.md)、進捗は [ロードマップ](rev-b-roadmap.md)。ここでは設計根拠、観測と未検証の境界を記録する。製造承認ではない。

## 外観を維持するための機構参照

オーナー指定は外形・キー間隔・スイッチ／スライダー位置を含む外観維持。OLEDはRev.Aと同じWaveshare 0.96inch OLED (C)（decisions §12）。

KiCad 10.0.3のpcbnewで既存ファイルを読み取り、以下を抽出した。座標は各ファイル内のfootprint原点であり、筐体寸法や部品中心を実測した値ではない。Edge.Cuts bounding boxは線幅も含むため、その値で新しい長方形を描き直さない。曲線・切欠き・穴は元の形状を参照する。

| 参照 | 抽出した値（mm） | 注意 |
|---|---|---|
| `hardware/kicad/Emiuet.kicad_pcb` | Edge.Cuts bbox 297.05 × 102.45 | Rev.Aの機構参照。電気回路はRev.Bと非互換 |
| 同PCBの78キー | 主な列間隔18、行間隔17 | col0→1は18.05。差0.05を無断で補正しない |
| 同PCBのSW1_0 / SW1_12 / SW6_0 | (313.85,51.2) / (97.8,51.2) / (313.85,136.2) | Choc hotswapの既存原点 |
| 同PCBのRV1 / RV2 / RV3 | (81.25,100.06) / (69.24,100.06) / (57.25,100.05)、各90° | 10k、既存35mm slider footprint。型番の購買照合は未完 |
| `Emiuet_plate/Emiuet_plate.kicad_pcb` | bbox 297.05 × 102.45、キーcutout pitch 18 × 17 | PCBと座標系・左右が異なる。組立照合前に座標を転記しない |
| `Emiuet_topLeft/Emiuet_topLeft.kicad_pcb` | bbox 24.05 × 102.449966 | 穴と形状を維持 |
| `Emiuet_topRight/Emiuet_topRight.kicad_pcb` | bbox 38.589997 × 102.489976 | slider cutout等を維持 |
| 同PCBのOLED Brd2 | (69.25,63.6)、B面、180°。外形26.1 × 26.1、7pin 2.54 mm、取付穴φ2.5 ×4（22.4 × 21.9間隔） | Waveshare (C) 26 × 26 mmと一致。plate/topRight cutoutも同形状 |

参照PCB SHA-256: `642400cf5f23b2d769d949b8072a8d1e20ea1ca32fac013d5ae9d9dc68474a3b`。
plate: `a884d3efe96c41c683d391f9426ecab2c7bc2a514b024a8397c090ecf8b00b1b`。
topLeft: `fe5e530ed70193449db73d8b35824400695cdb161e0ee251ac769437b036b4e0`。
topRight: `08d0631b87b2ed6dc95a61d2c5c71e473350d10ceeefdf5252005be4c78a843a`。

PCBとplateは同じviewではないため、sliderが左右反対に見えることだけで誤りと断定しない。表裏・組立原点を合わせた重ね合わせは未実施。旧電池／role用スイッチの位置維持と電気的用途、新MIDI IN開口は別問題であり、未定義機能を割り当てない。

## 実回路化したブロックと根拠

| ブロック | schematic上の構成 | 未完の条件 |
|---|---|---|
| ESP32-S3 reset/BOOT | R1 10k + C2 1uのEN RC、SW1 RESET、R2 10kとSW2 BOOT、C3 10u/C4 100n | GPIO46 strap、実電源立上り、switch footprint、電源設計を照合 |
| USB-C CC検出 | R8/R9 5.1k 1% Rd、R10/R11 100k + C7/C8 10n、R12 38.3k/R13 10k/C9 100nのVREF、U6 TLV7022DGKR、R6 10k pull-up、C5 100n bypass | CC ESD、Rd/VREFのMPN、実機の応答時間 |
| I2C / OLED | R4/R5 4.7k、J4 Waveshare 0.96inch OLED (C)（Rev.A footprintを流用） | module側pull-upとの合成値、RES/DC/CS開放の実機確認 |
| RGB buffer | U4電源/OE/GND接続、C6 100n、R7 68Ω直列、R14 100k RGB_DATA_3V3 pulldown | 68Ωは候補。各pixel bypass、bulk、信号波形は未検証 |

根拠はメーカー一次資料: [Espressif schematic checklist](https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32s3/schematic-checklist.html)、[TLV7022 Rev.F](https://www.ti.com/lit/ds/symlink/tlv7022.pdf) §5、6.1、6.10、[SN74AHCT1G125 Rev.P](https://www.ti.com/lit/ds/symlink/sn74ahct1g125.pdf) §8.3（入力をfloatさせない）、[Waveshare 0.96inch OLED wiki](https://www.waveshare.com/wiki/0.96inch_OLED_Module)。ENの10k/1uは推奨初期値で、全電源条件を保証する値ではない。

追加passiveは0603 footprintを仮指定したが、MPN・定格・DC bias・供給性は未確定。SW1/SW2は回路上の参照番号で、Rev.A PCBの同番号とは同一部品を意味しない。U6はKiCad標準のLM2903 symbol（LM393と同じpin配置: OUTA 1、INA− 2、INA+ 3、VEE 4、INB+ 5、INB− 6、OUTB 7、VCC 8）で描き、TLV7022 Pin Functionsと照合した。

## USB-C電流検出の設計

TUSB320を撤去した理由はdecisions §9とhistoryに記録した。ここでは置換回路の根拠を残す。

- **閾値:** Type-C sinkのvRd範囲はDefault 0.25–0.61 V、1.5 A 0.70–1.16 V、3 A 1.31–2.04 V。規格上の判定閾値0.66 Vに対し、VREFを0.683 V（3.3 V × 10k/48.3k）とした。3.3 V ±1.5%、抵抗1%、offset ±8 mV、hysteresis最大15 mVを重ねた判定点は約0.645–0.72 V。Defaultを1.5 Aと誤認する側（0.61 V以下で判定）には約35 mVの余裕がある。代償として、0.70–0.72 V付近の正規1.5 A sourceをDefault扱いにし得る（明るさが下がるだけ）。offset/hysteresisの規定条件はVCM = VS/2で、動作点0.68 V付近の値は未確認。3.3 V regulatorの精度が決まったら再計算する。
- **極性:** IN+ = VREF、IN− = CCx。どちらかのCCが閾値を超えるとopen-drain出力がLowになり、2出力のwired-ORで `USB_CC_1A5_N` を作る。comparator未給電（POR中Hi-Z、§7.4）と出力開放はpull-upでHigh（Default）に倒れる。出力段のLow固着、GPIO37のGND short、CCx_FILTがVREFより高い電位へshortした場合は1.5 A側になる。
- **未給電時:** TLV7022の入力はVCCと無関係に5.5 Vまでfault tolerant（§7.4.1）、絶対最大はVEE基準で7 V（§6.2）。Sourceは3.3 Vが立ち上がる前からCCにRp（最大約2.04 V）を出すが、VCCへの逆流経路はない。R10/R11 100kでさらに電流を制限する。
- **PD通信の除去:** PD SourceはSink応答がなくてもCC上でBMC通信を送る。100k/10n（τ 1 ms）で平滑化し、firmwareで上昇方向を100 ms安定まで待つ。Rd側から見た追加容量はR10/R11で切り離される。
- **応答時間:** Type-Cはsinkに、Rp変化から60 ms以内（tSinkAdj）の電流低下を求め、変化判定に10–20 ms（tRpValueChange）を許す。firmwareは10 ms周期で読み、低下を2 sampleで反映する（最大約20 ms）。renderer周期16 ms（tick 1000 Hz）とRMT送出（約2.4 ms）を足した最悪見積もりは約38 ms。host simulationでは低下反映17 ms。実機の測定は未実施。
- **scheduling:** `usb_cc` taskは `configMAX_PRIORITIES - 1` とし、他taskの挙動に左右されないようにした（処理は10 msごとに数µs）。core 0のMIDI送信taskがidle時に回り続ける問題は、通知方式への変更で解消した（ロードマップ「確認状況」）。rendererはcore 1の優先度4で、core 1に常時回るtaskはないと読んでいる。実機でtask WDTと応答時間を確認する。
- **PD通信中の誤ったDefault判定:** 1.5 A/3 A sourceがBMCを送っている数msの間は、filter後の電圧が判定点を下回り得る。2 sample連続が必要なため、10 msを超えるburstでなければDefaultへ落ちない。この誤判定は安全側。PD sourceでのCC波形は実機で確認する。
- **残る保護:** CC線のESDは、USB D+/D−と同じinput protection blockで扱う（未選定）。

## USBとLED電力の状態

| 状態 | firmwareのRGB budget |
|---|---|
| 起動直後（CC判定前） | 0 |
| Default、USB未configuredまたはsuspend | 0 |
| Default、configured、非suspend | 200 mA（Kconfig既定） |
| Rp ≥ 1.5 A（3 Aを含む） | 1000 mA（Kconfig既定）。3 Aへ増やさない |

budget 0はrender出力を黒にするだけで、LED素子の待機電流やMCU/OLEDの電流は残る。USB configuration descriptorのbMaxPowerは250 mAから500 mAへ変更した。Defaultで動く全体の電流（MCU、OLED、78 pixelの待機電流、LED 200 mA）は250 mAを超える見込みであり、宣言と実消費を一致させるため。

Default 500 mAの内訳は、MCUのUSB/BLE動作電流とSK6812 MINI-Eの待機電流が未測定のため、まだ閉じていない。OLEDは全点灯で約25 mA（Waveshare wiki、3.3 V）。V0–V2でこれらを測り、200 mA設定を確定または下げる。1.5 A/3 A時のsuspend電流の扱いは、[USB-IF functional tests](https://www.usb.org/sites/default/files/USB%20Type%20C%20Functional%20Test%20Specification%202024%2003%2003.pdf) のTD4.10系列とValidation時に照合する。

## 3.3 V電源の選定で残っていること

| 案 | 設計上の特徴 | 現時点の扱い |
|---|---|---|
| AP2112K-3.3系LDO | 5→3.3 Vで0.3 Aなら約0.51 W、0.5 Aなら約0.85 Wを発熱 | schematicのplaceholder。定格だけで採用しない |
| TPS62162等のbuck | 3.3 V固定・1 A級の候補。inductor/capacitorとlayoutが必要 | 候補比較まで。採用・在庫確認は未完 |

根拠: [AP2112 datasheet](https://www.diodes.com/datasheet/download/AP2112.pdf)、[TPS62162 datasheet](https://www.ti.com/lit/ds/symlink/tps62162.pdf)。発熱値は単純な(Vin−Vout)×Iの計算で、温度予測ではない。ESP32-S3の電源能力要求、OLED等の負荷、USB総電力、温度条件を揃えて選定する。input protection、inrush、LED電源の遮断手段も同じ電力設計で決める。

## 確認結果と限界

111 components。KiCad 10.0.3でERCは35 errors / 167 warnings（初期50 / 174）。残りはregulator・MIDI未接続などに由来し、今回のブロックからは新しい違反は出ていない。CC検出、OLED、R14の各pinの接続はXML netlistで確認し、描画も目視で確認した。ERCをignoreで消してはいない。未接続電源・MIDI等が残るので製造不可。

`usb_power.c` と `usb_cc_detect.c` の実ソースを、一時的なFreeRTOS/GPIO stubとMSVC `/W4 /WX` でcompileした。そのうえでCC pinを模擬し、次のassertがPASSした。

- 起動直後
- 上昇方向のdebounce
- 低下時の応答時間
- 1 sampleだけのglitchの除去
- configured/suspend別のbudget

これは単体ロジックの確認。ESP-IDF 5.3.4のRev.B構成buildは成功した。TinyUSBとの実動作と実機はUNVERIFIED。

独立レビュー1回目（2026-10-05）は、TUSB320版の差分を対象に行った。指摘の扱いは次のとおり。

- TUSB320LAIの型番とアドレスの不一致、Rp追従の遅れ、周期消灯、INT_N未clear: TUSB320の撤去で解消。
- RGB_DATA_3V3のfloat: R14を追加して対応。
- descriptorの過少宣言: 500 mAへ変更。
- DETACHEDとstate taskの競合: 書き込みをstate taskだけにして解消。
- verify toolのlibrary path優先: CLIと同じinstallを常に使うよう修正。
- 未対応で残るもの: 電源rail未接続、footprint未割当、EN_RESET等の残りERC。

独立レビュー2回目（同日）は、置換後のCC検出・OLED・firmware・文書を対象に行った。netlist、TLV7022のpin配置と極性、VREF、OLED footprintの配置復元（11 padとbboxが一致）は問題なしとされた。指摘の扱いは次のとおり。

- core 0の飢餓（blocker）: `hid_keyboard_tx` は0 tick待ちのため、USB未mount中に回り続けていた。1 tick待ちに修正した。`usb_cc` は最高優先度へ上げた。MIDI送信taskのidle `taskYIELD()` loopは、オーナー承認後に通知方式へ変更した。
- 判定点上限0.72 V、5.5 V fault toleranceの表記、故障モードの区別、PD中の誤ったDefault判定: 上の設計節に反映した。
- auditとREADMEの食い違い、footprintに残ったboard由来の値、`stable` のwrap、stackの余裕: 修正した。

tSinkAdjとtRpValueChangeは二次情報（Zephyrの仕様表引用）で照合した。vRdの範囲と0.66 V閾値は一次資料で未照合。USB Type-C仕様本文での照合は、3.3 V regulator決定時のVREF再計算と同時に行う。
