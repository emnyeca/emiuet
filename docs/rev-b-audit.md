# Rev.B 設計・開発環境監査

監査日: 2026-10-05。対象: `feat/revb-usb-powered-rgb`、`1fff3fd141df0f5aa6b7872cf6fc79add14f0f3e`。
開発者・AI向けの現状評価と移行提案であり、製品仕様の第二の正本ではない。有効な判断は [decisions.md](decisions.md)、GPIO割当は [pinout-v3.md](pinout-v3.md) を参照する。以下の状態は監査時点の記録である。特に、TUSB320をCC detectorとして残す前提（§1–3）は、その後decisions §9でRd＋comparatorへ置換された。

## 1. 結論と現在地

**Rev.Bはアーキテクチャと一部firmwareが先行し、製造可能な回路設計はこれからの段階。** 回路図は存在するが、電源・MIDI・入力系の実回路が未完成であり、「部品値調整とERC cleanupだけで完成する」状態ではない。既存PCB/BOMをRev.Bとして使うことはできない。

- 残す: guitar-firstの6×13配列、演奏表現、single USB-C Device/UFP、native USB、外部USB給電、5V RGB＋3.3V logicという設計意図。firmwareのtransport分離とGPIO割当も照合の出発点になる。
- 作り直す: 電源・保護・reset/BOOT・MIDI・matrix/UIを含む完全なschematic、部品選定、PCB、BOMと製造出力。既存draftを修復するか新規に起こすかは、小さい差分に固執せず読める成果で決める。
- 持ち越さない: 内蔵電池、charger、PowerPath、boost、dual USB、Host VBUS、旧PCBの接続。Rev.Aの完全な別系統保存や履歴の追加整理には時間を割かない。
- EDAは**KiCad継続を第一推奨**。ただしschematic編集APIの制約は実在する。EasyEDA Proは有力な対抗候補で、単に「自動化できない」として除外できない。判断根拠と小規模な比較条件を後述する。

今回はauditに限定し、schematic/PCB/firmwareは変更していない。Rev.A資産も移動・複製していない。

### 調査の範囲

root/firmware/hardwareのREADME、docs全体の関連記述、GPIO/USB/CC/RGB実装、KiCad source・library tables・製造BOM、直近の設計変更commit、branch/tag、GitHub PR/issue一覧を確認した。vendored u8g2の全履歴やRev.A配線全体は追跡していない。既存validationとの重複を避けるため、隣接 `emiuet-validation` のREADME、validation matrix、VAL-CORE-01 READMEも確認した。

重要な変更は `2ad9825` (2026-08-31: Device-only)、`03ac33d` (2026-09-01: USB給電/RGB)、`1fff3fd` (2026-09-02: 製品思想・文書・CC7転用の修正)。ローカルtagはなく、[PR #3](https://github.com/emnyeca/emiuet/pull/3) はOPEN、#1/#2はMERGED。取得時のissue一覧は空。mainへのmerge有無を、ローカル設計の有無と混同しない。

## 2. 要件・判断の再構成

`CONFIRMED` は有効な意思決定または実ファイルで確認できた事実であり、実機合格を意味しない。`LIKELY` は推定、`LEGACY` は撤回済み設計、`CONFLICT` は不一致、`UNDECIDED` は未確定。IDは本audit内の追跡用であり、新たな仕様体系を導入したものではない。

| Requirement / Decision | Current interpretation | Status | Evidence | Conflicts | Action |
|---|---|---|---|---|---|
| MUS-01 配列・演奏 | 6×13、上方向PB、自動center、限定stringwise MPE | CONFIRMED | decisions §1–4、board_pins、matrix/input_router | schematicに実matrixなし | 78キー/diode回路を実装し演奏負荷で確認 |
| MUS-02 sliders/UX | PB/MOD/VELの3本、minimal OLED、button×3 | CONFIRMED | README、pinout、slider/controls/ui_oled | ADC filter/OLED接続がdraft注記 | analog値・機構・ノイズ合格条件を選定 |
| USB-01 | single USB-C、Device/UFP、MIDI＋HID composite、native flashing | CONFIRMED | decisions §7–8、midi_out_usb、J1/U1 | 保護・recovery回路未完 | USB/BOOTの実回路化 |
| HID-01 | TYPEは補助、4隅2秒、再列挙せず切替 | CONFIRMED | keyboard-mode、input_router | 実機統合は未検証 | 演奏/HID release境界を維持 |
| MIDI-01 | Type-A TRS OUT＋isolated IN、USB、BLE capability | CONFIRMED | decisions §6、UART/USB実装 | J2/J3/U5未接続、BLE stub | OUT/INを設計、BLE完成と宣伝しない |
| BLE-01 | BLE MIDI/Custom Servicesは将来実装を拘束する設計資料 | CONFIRMED / UNDECIDED | docs/BLE、midi_out_ble | normative記述に対しUUID等はplaceholder | 有効な方針を保ち、未確定protocolを別扱い |
| MCU-01 | ESP32-S3-MINI-1-N4R2、GPIO26不使用 | CONFIRMED | pinout、board_pins、U1、Espressif資料 | footprint未選定 | exact MPN・land pattern・antenna keepout照合 |
| PWR-01 | 外部USB 5V、内蔵battery/charging/PowerPathなし | CONFIRMED | decisions §8/11、sch部品構成、history | 旧PCB/BOMには電池系 | 旧電源を再採用しない |
| PWR-02 | 5V LED、3.3V MCU/OLED/logic、約1.5A製品上限 | CONFIRMED / UNDECIDED | decisions §8、U3候補 | regulator・保護・実電流未確定 | 負荷/温度/起動から選定、上限を定格保証と呼ばない |
| USB-02 CC | TUSB320固定UFP、attach/current検出、3Aでも1.5A上限 | CONFIRMED | decisions §9、tusb320、TI Rev.F §7.3.1.2 | error時予算更新とUSB状態連携なし | fault時縮退、Rp低下応答を設計 |
| RGB-01 | SK6812 MINI-E×78、GPIO38/AHCT、single chain | CONFIRMED | decisions §10、D1–D78、led_renderer | buffer等未接続、local bypassなし | 実回路/電流モデル/物理mappingを完成 |
| ADC-01 | ADC1 GPIO1/2/4、slider noise抑制 | CONFIRMED / UNDECIDED | pinout、slider、旧design notes §4 | RC/slider値未確定 | 許容noiseと応答時間を決める |
| PROT-01 | VBUS/CC/USB/外部端子の保護、inrush/短絡対策 | UNDECIDED | hardware README、sch注記 | 部品・配線が無い | exact partsと帰路をdatasheetから設計 |
| DBG-01 | GPIO0 boot、EN/reset、native USB recovery | CONFIRMED / UNDECIDED | pinout、旧notes §3、Espressif guide | 現行draftに必要周辺回路なし | EN/BOOTを実装しbad firmware復旧を確認 |
| MECH-01 | 薄型、78 low-profile keys | CONFIRMED / UNDECIDED | README、旧PCB/plate/topLeft/topRight | Rev.B外形寸法・層数・筐体拘束未確定 | 旧寸法を転記せずkey pitch/LED干渉/端子位置を確認 |
| MFG-01 | JLCPCB向け旧製造出力あり | LEGACY / UNDECIDED | production、hardware/bom | Rev.B BOM/PCBA・手実装境界なし | switch/hotswap/LED向き・実装面と調達を決定 |
| MFG-02 | Basic/Extended採用規則 | UNDECIDED | 現行decisions等に明示方針なし | 在庫・単価の監査未実施 | 安全/供給/総実装費で判断し、Basic優先を捏造しない |

## 3. 不一致と優先して解消するリスク

1. **READMEの「architecture draft」は正しいが、未完成範囲が小さく見える。** `Emiuet.kicad_sch` は87部品（LED78＋その他9）。matrix、slider、OLED、保護、reset/BOOT、各LED bypassは実回路になっていない。部品選定前に接続設計そのものが必要。
2. **電流制限を安全と呼べる根拠が不足。** `usb_power.c` はDefault/unknownをLED 200mA、1.5A/3Aを1000mAとする。sink attach状態を予算条件にせず、`tusb320.c` のread失敗時には前回値を保持する。`midi_out_usb.c` のdescriptor引数は250mAだが、MCU/OLED等を含む全機電流との対応がない。未列挙・configured・suspend・power-bank・Rp低下・I2C故障を区別した電力状態表が必要。USB適合はUNVERIFIED。
3. **LEDの計算値は実電流上限ではない。** `led_renderer.c` は60mA/pixelのRGB比例モデルで、idle電流、部品variant、過渡、MCU負荷を計測していない。電流モデルを実測で補い、fault時の縮退とhardware保護を合わせて設計する必要がある。
4. **PR #3本文は古い。** CC7をglobal brightnessへ転用すると書かれているが、`1fff3fd` の `led_control.c` とdecisionsでは撤回済み。PR本文を仕様根拠に戻さない。今回外部のPR本文は編集していない。
5. **GPIO正本の表現が重複。** pinout文書と `board_pins.h` の双方がsource of truthを名乗る。今回の割当値は一致するが、変更時はpinoutを設計割当、headerをその実装として区別し、schematic net/pinと照合する。
6. **履歴化で隠れた継続要件がある。** `history/rev-a-design-notes.md` のboot安定性、EN、ADC noise等は旧電池方式とは独立した設計根拠。旧revision名だけで廃止しない。`.github/copilot-instructions.md` のdocs一律優先、最小差分優先も、current/historyの区別と今回の再設計指示に照らして読む。
7. **再現性の障害。** `sym-lib-table` と `fp-lib-table` の一部が旧 `F:/…` 絶対パス。埋込symbolで開けることはlibraryが再現可能な証拠ではない。project-localまたはversion固定の共有libraryへ解決する必要がある。
8. **BLE資料の読者と言語。** docs/BLEはfirmware/app開発者向けだが英語で、normative方針とplaceholderが同居する。将来編集時に日本語へ整理し、有効な要件はhistoryへ落とさない。未完成BLEをPCB必須変更の理由にしない。

## 4. 既存hardwareの判定と独立検証

| 対象 | 分類 | 利用可能な範囲 |
|---|---|---|
| `hardware/kicad/Emiuet.kicad_sch` | Rev.B draft | USB給電/RGB/GPIOの意図。完成回路として流用不可 |
| `Emiuet_RevA.kicad_sch` | Rev.A legacy | 旧回路・機構を確認するときだけ参照 |
| `Emiuet.kicad_pcb`、main production、`hardware/bom/Emiuet_RevA_BOM.xlsx` | Rev.A legacy | 旧製造物。Rev.B発注不可 |
| plate/topLeft/topRightと各production | legacy機構参照 | Rev.Bへの適合はUNVERIFIED。部品変更/LED追加後に干渉確認 |
| `Emiuet.kicad_pro` / library tables | mixed | 現行schと旧PCBを同じprojectが指す。旧絶対library参照あり |

ファイル名だけでなく実体を確認した。旧PCB/BOMにはBQ24074、TPS61023、LM66100、battery、USB-C×2があり、現行schにはそれらがない。一方、Rev.B schにはTUSB320、78 LEDがあるため、全体をRev.Aと分類するのも誤り。

独立VerifierがKiCad **10.0.3** (`C:/Program Files/KiCad/10.0/bin/kicad-cli.exe`) で現行schのERCとnetlist exportを実行した。ERCは **FAIL: 50 errors / 174 warnings**。この件数には接続不備と配置上の警告が混在し、全てを同じ種類の電気故障とは数えない。重要なnetlist事実は次の通り。

- U3 regulatorの5端子が未接続。
- U4 bufferはinput pin 2とMCU pin 34の接続のみで、電源/GND/output等が未接続。D1 DINはundriven。
- U5の8端子、J2/J3 TRSの全端子が未接続。
- 非LED9部品のinstance footprintが空。matrix/UI/protection等は注記に留まる。

再現コマンド（出力先は任意の一時directory。project sourceは保存しない）:

```powershell
$kicadCli = 'C:/Program Files/KiCad/10.0/bin/kicad-cli.exe'
& $kicadCli sch erc --exit-code-violations --output "$env:TEMP/emiuet-audit-erc.rpt" hardware/kicad/Emiuet.kicad_sch
& $kicadCli sch export netlist --output "$env:TEMP/emiuet-audit.net" hardware/kicad/Emiuet.kicad_sch
```

Rev.B PCBが無いためRev.B DRC/parityはUNVERIFIED。旧PCBを更新して整合させる操作はしない。実機の電圧/温度/USB/EMI、footprint適合、firmware integration buildも本auditではUNVERIFIED。既存host testsだけでhardwareをPASSにしない。

## 5. EDA再評価

2026-10-05に一次資料を確認。以下は資料上の能力とこのprojectでの判断であり、EasyEDA/atopileを実機操作した比較ベンチマークではない。

| 評価軸 | KiCad 10 | EasyEDA Pro | atopile＋KiCad |
|---|---|---|---|
| AIによるschematic編集 | text構造へアクセス可能だが10のIPCはPCB Editorのみ。生成/編集後のnetlist・描画照合が必要 | 4.1.60以降のdesktop CLIからextension API、SCH/PCB/libraryへアクセス | `.ato`で接続/制約を記述。導入する言語とcompilerが増える |
| PCB操作 | IPC API＋GUI。10ではheadless IPCではない | PCB APIあり。editor/sessionへの依存を確認する | KiCad layoutを利用 |
| ERC/DRC/parity・CI | CLI ERC/DRC、schematic-parity、export。現在の環境でERC実行済み | schematic/PCB check API。clean runnerでの起動・保存・完了待ちを実証する必要 | build/constraints/checks＋KiCad DRC。通常ERCと同等と推定しない |
| Git/diff/人間review | text差分＋PDF/SVG＋netlist。UUID/座標差分だけでは読みにくい | local project/exportとtext `.eprj3`の手順あり。Git不可能ではないがround-trip差分を評価 | code差分は読める。生成物とコードの二重編集を避ける必要 |
| BOM/LCSC/JLCPCB | supplier field/CSV/position/Gerberを明示管理 | 部品library/製造連携が候補上の利点。MPN・pad適合を別途確認 | buildからBOM/製造出力。供給選定とversion固定を確認 |
| library/再現性 | project-local資産とtool version固定が可能 | local library/exportの完結性・offline復元を確認 | package/compiler/library固定が追加で必要 |
| lock-in/保守 | open format、既存閲覧/レビュー手段が利用可能 | vendor仕様とAPI変更への依存。local exportは軽減策 | open toolchainだがDSL/compilerへの依存が増える |
| 安全なAI編集 | APIまたは構造化編集、小単位差分、描画/netlist/ERCを合わせる | APIでも接続/保存/DRC/描画まで確認 | constraint通過だけで回路・layout安全を保証しない |

根拠: [KiCad 10 CLI](https://docs.kicad.org/10.0/en/cli/cli.html)、[IPC APIのversion別境界](https://dev-docs.kicad.org/en/apis-and-binding/ipc-api/for-addon-developers/)、[EasyEDA Pro CLI/API・local text project](https://prodocs.easyeda.com/en/api/guide/cli.html)、[local export](https://prodocs.easyeda.com/en/project/file-save-as-local/)、[atopile公式](https://github.com/atopile/atopile)。KiCad 11向けheadless IPCを10の機能として扱わない。

**推奨理由:** 本件では、人間が回路を監修できること、ローカルで再現できること、検証と製造出力を自動化できることの合計を優先する。KiCadはその検証経路を既に実行でき、schematic作成の制約はあるものの、全面移行を今すぐ選ぶ根拠は不足する。既存回路の保存価値を理由にしているわけではない。

次の設計着手時、電源/MCUの小ブロック一つを「AI作成→再読込→netlist/ERC→PDF→人間による接続変更」で試す。KiCadの編集・reviewコストが支配的なら、同じブロックをEasyEDA Proで比較する。判定項目は接続の一致、offline再現、安定したGit差分、BOM/footprint照合、所要手間。atopileは反復回路が増え、コード正本を人間が監修できる見込みが立った場合の候補に留める。

製造はKiCadからJLCPCB向け出力を生成する方式をまず選ぶ。EasyEDA変換を必須の中間工程にするとsymbol/footprint/net変換の照合負担が増える。hybridを採るなら製造用一方向exportに限定し、双方を編集正本にしない。

## 6. Rev.Bアーキテクチャの推奨

```text
USB-C ×1 ─ D+/D- protection ─ ESP32-S3 native USB (MIDI/HID/recovery)
  ├ CC1/CC2 ─ TUSB320 fixed UFP ─ I2C/current status ─ firmware budget
  └ VBUS ─ protection/inrush/current control ─ 5V ─ 78 RGB
                                                ├ AHCT buffer ← GPIO38
                                                └ 3.3V regulator ─ MCU/OLED/logic
MCU ─ 6×13 diode matrix / slider ADC×3 / buttons×3 / pilot LED
    ├ UART TX ─ compliant driver ─ Type-A TRS OUT
    └ UART RX ← isolated receiver ← Type-A TRS IN
```

Host/DRP/dual USB/battery/charger/boost/PowerPathは復活させない。native USBを使うため外部USB data controllerは不要。TUSB320はUSB data controllerではなく、78 LEDの給電予算に必要なCC detectorとして残す現行判断に理由がある。抵抗Rdだけに替えるには固定の低電力運用という別の製品判断が必要。

5V LEDを選んだ現行条件では3.3V rail一本への統一は推奨しない。3.3V regulatorは未選定としてLDOとbuckを比較する。LDOなら損失は `(5−3.3)×I`、例えば0.3Aで0.51Wとなるため、部品数だけで選ばず温度・電圧降下・radio peak・ADC noiseを評価する。これは計算例であり負荷実測ではない。

MIDI OUTは既存の5V driver方針を出発点にし、不要な段数を減らす。3.3V対応の標準回路が存在することと、MCU GPIOをそのままTRSへつなげてよいことは別。rail変更は[公式MIDI電気仕様](https://midi.org/5-pin-din-electrical-specs)とType-A割当、source/sink定格を照合してから判断する。INの絶縁を部品削減で失わない。

### 部品根拠を確定する順序

| 対象 | 現在の理由・代替 | 設計を確定する前の証拠 | 状態 |
|---|---|---|---|
| ESP32-S3-MINI-1-N4R2 | native USB/BLE/GPIO。module変更は配列・FWへ波及 | exact datasheet、land pattern、antenna、EN/BOOT、errata、電源peak | 方針CONFIRMED、実装UNVERIFIED |
| TUSB320 | current検出。代替は同等CC detectorまたは固定低電力/Rd案 | exact suffix/package、dead-battery/供給電圧、pull-up/strap、reset中挙動 | 方針CONFIRMED、接続/故障応答UNVERIFIED |
| 3.3V regulator | draftの候補値を採用品としない。LDO/buck比較 | peak負荷、熱、dropout、stability、推奨C、起動 | UNDECIDED |
| SK6812 MINI-E/AHCT | 78 key可視化と3.3→5V。選定variantの確認が必要 | メーカーdatasheetのpin向き/電流/timing/Vih、buffer OE/power-off挙動 | footprint/電流UNVERIFIED |
| USB保護/MIDI optocoupler等 | 回路用途を先に固定してMPNを比較 | ESD容量/耐圧/クランプ、CTR/遅延、抵抗電力、connector定格 | UNDECIDED |

一次資料: [ESP32-S3-MINI-1 datasheet](https://documentation.espressif.com/esp32-s3-mini-1_mini-1u_datasheet_en.html)、[ESP32-S3 hardware guide](https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32s3/schematic-checklist.html)、[TUSB320 Rev.F datasheet](https://www.ti.com/lit/ds/symlink/tusb320.pdf)。TI §7.3.1.2は接続中の電流検出更新にperiodic I2C soft resetを要求しているが、現在の2秒周期と故障時処理まで合格とする根拠にはならない。Espressifの電源能力推奨をそのままUSB実消費電流と読み替えない。

未選定品の最新datasheet/guide/reference/errata/application note、JLCPCB/LCSC在庫は未確認。MPN選定時にメーカーURL・document revision・参照節・取得日、電気制約、代替、供給確認日を同じ部品記録へ残す。値・接続が決まるまではverifiedにしない。

## 7. AI workflowとrepositoryの移行案

新しいrequirements/architecture/datasheets等の空directoryを一括作成しない。`docs/decisions.md` は判断、`docs/pinout-v3.md` は割当、`hardware/kicad/` は電気設計、schematic fieldはMPN/値/footprint/LCSC、productionは生成物という既存構造を活かす。要求IDは給電、boot、MIDI、演奏latencyなど検証へ結び付く高影響項目からのみ導入する。

重要判断はdecisionsの該当節にcontext/options/chosen reason/consequencesを短く統合する。日付・根拠は判断の由来が必要な場合に記載し、各変更ごとの大量ADRは作らない。部品選定理由は次回の選定成果を一つのhardware設計ノートにまとめ、schematicと同じ値の表を手入力で複製しない。

1. **Designer:** 一つの回路ブロックをdatasheetから構成。接続、値、footprint、未決事項を明示し、schematicの見た目まで出す。
2. **Verifier:** 別agentがnetlist/pinout/電源/footprintを独立照合し、ERC/DRC結果とtool versionを記録。Designerの説明を根拠に代用しない。
3. **Reviewer:** 回路設計が間違っている可能性から、reset、帰路、ADC noise、ESD、USB、温度、製造を検討。少数の高影響指摘へ絞る。
4. **人間:** 製品方向と未決tradeoffを判断し、回路/PCBと測定結果を監修。自動チェック通過を製造承認にしない。

今回も別Verifierが実ファイルからERC/netlistを確認した。回路を設計した後の独立Reviewerは、次の設計反復で行う。役割を分けるためだけに常時3agentを走らせる必要はない。

自動化はKiCad version/library固定→ERC/netlist/PDF/BOM→新Rev.B PCBでDRC＋schematic-parity→Gerber/drill/positionという順。CIは元sourceを変更せず、tool version・commit・reportをartifact化する。FAILは検出違反、WARNは判断付き留保、UNVERIFIEDは未実行/証拠不足、PASSはその検査範囲の合格。未完成schematicのERCを無理に緑にするための一括除外はしない。

今回CIや新規test suiteは追加しない。監査段階では既存CLIを実行すれば足りる。保守対象の新規テストを導入するときはgovernance §4.3に従い、守る境界と追加しないリスクを提示する。

## 8. 物理validationとロードマップ

物理validationは既存の [emiuet-validation](https://github.com/emnyeca/emiuet-validation) のVAL-CORE-01とV0–V3を利用する。電池用試験を戻さず、新しい並列validation体系を作らない。validation boardのphysical power switchを、製品の確定要求へ自動昇格させない。

| 測定点/対象 | 目的・期待範囲の決め方 | 既存対応 | 今回の実測 |
|---|---|---|---|
| TP_USB_VBUS、TP_5V、遠端5V、GND | 入力/配電drop、inrush、負荷step。connector/LED/regulator条件から数値化 | V0-02、V2-05/06、V3-03 | 未測定 / UNVERIFIED |
| TP_3V3、電流測定用経路 | MCU peak時の電圧/温度、全機電流予算 | V0-02、V3-06 | 未測定 / UNVERIFIED |
| EN/BOOT | 立上り、通常boot、download、bad firmware復旧 | V0-04/05/06 | 未測定 / UNVERIFIED |
| CC1/CC2、I2C | attach、Rp低下、通信故障、reset中の予算 | V1-06/07、V2-04 | 未測定 / UNVERIFIED |
| USB D+/D- | 列挙/波形/復旧。測定用stubを増やさず適切にprobe | V1-01、V3-04 | 未測定 / UNVERIFIED |
| LED data 3V3/5V、first/last pixel | Vih/timing、mapping、供給変動 | V2-01/02、V3-02 | 未測定 / UNVERIFIED |
| matrix/ADC/UART/OLED | 同時負荷でscan/sliderノイズ/MIDI電気条件/I2C安定性 | V1、V3-01/05/06 | 未測定 / UNVERIFIED |

期待値の数値が未決の試験は、通電前にdatasheet限界と製品の許容latency/noiseから範囲を確定する。既存results templateへdesign value、expected range、measured value、PASS/FAIL、基板/BOM/FW commit、電源/ケーブル、測定器、波形を戻す。6 LEDでの合格を78 LED配電の合格と扱わない。

| 順序 | 実行内容 | 次へ進む条件 |
|---|---|---|
| 1 | EDAの小ブロック作成・再読込実証、library絶対パス解消 | 人間が接続を読め、clean環境で再現可能 |
| 2 | USB全機電力状態表、regulator/保護/MIDI部品選定、機構拘束確認 | 未列挙/suspend/Rp低下/fault時の挙動と予算、MPN根拠が揃う |
| 3 | Rev.B schematicを少数ブロックで実装。FWの予算状態を整合 | independent review、ERC、netlist/GPIO/footprint照合 |
| 4 | VAL-CORE-01で電源/MCU/I/O/RGBを確認 | V0–V2の合否と数値が記録される |
| 5 | Rev.B PCBを新設計、78 key/LEDの機構・帰路・熱を確認 | DRC/parity、antenna/USB/ADC/配電review、BOM/DFM |
| 6 | 製造出力と試作、V3統合確認 | 同一commitから出力、実測で性能・安全境界を確認 |

アムニェカさんの判断が必要なのは、EDA変更の必要性が実証された場合の選択、Rev.B外形/層数/実装コストのtradeoff、選定後の給電条件と許容輝度である。現行single USB給電方針の再承認やRev.A全保存の承認を、今回の前進条件にはしない。
