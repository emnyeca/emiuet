# Rev.B 開発ロードマップ

最終更新: 2026-10-05。開発者・AI向けの進捗と再開地点の正本。Codex、Claude Codeなど、chatやAIを替えてもこの文書から再開する。

製品判断は [decisions.md](decisions.md)、GPIOは [pinout-v3.md](pinout-v3.md)、初期調査は [監査](rev-b-audit.md)、追加回路の根拠・残課題は [回路ノート](rev-b-circuit-notes.md)。仕様の正本を増やさない。

## 目指すところ

6×13の演奏体験とRev.Aの外観・操作部配置を維持し、USB給電専用のRev.Bを完成させる。KiCad上の実回路を小さな単位で設計し、接続検証・描画・独立レビュー・実測を重ねる。Rev.A保存のための大規模再編は行わない。

## 一目で見る進め方

| 順番 | 到達点 | AIの作業 | アムニェカさんの作業 | 状態 |
|---|---|---|---|---|
| 1 | 別のchat・AIから再開できる | ロードマップと共通入口を整備 | 同じ作業フォルダを開く | 完了 |
| 2 | 回路を編集・機械検証・目視確認できる | KiCad依存整理、ERC/netlist/BOM/SVG出力 | 図の読みやすさを監修 | 出力環境は完了、回路合格は未達 |
| 3 | USB給電条件と部品選定の根拠が揃う | 状態表、故障時FW、電源候補比較 | CC検出方式・OLED型番は回答済み | CC検出は設計済み、3.3 V・保護・電力内訳が未完 |
| 4 | 接続の揃ったRev.B schematic | MCU/USB/電源/MIDI/matrix/UI/RGBの設計と独立レビュー | tradeoffと回路を監修 | reset/BOOT・CC検出・OLED・RGB buffer済み、電源/MIDI/matrix/slider未完 |
| 5 | 小基板で電源・信号を実証 | 既存VAL-CORE-01へ反映、測定手順 | 製造判断・組立・V0–V2測定 | 回路確定待ち |
| 6 | 外観を維持した78キーRev.B PCB | 機構照合・配置配線、DRC/parity/DFM | 組立性と発注を確認 | 小基板の結果待ち |
| 7 | 楽器として使えることを実証 | 測定結果の解析・修正 | 78キー負荷・演奏・連続運転のV3測定 | Rev.B試作待ち |

ERC合格、実機合格、製造承認は別々に記録する。実測前はUNVERIFIED。

## 現在の再開地点

**次は手順3の3.3 V regulator・入力保護・Default電力内訳を決め、手順4の残りブロックを実回路化する。** 外形、キー間隔、スイッチ／スライダー位置などの外観は維持する。OLEDはRev.Aと同じWaveshare 0.96inch OLED (C)に決定済み。

- オーナー判断で、TUSB320をRd＋TLV7022 comparatorへ置換した。TUSB320はType-CのtSinkAdj（60 ms）を満たせないため。decisions §9、history、pinout、firmwareに反映済み。
- schematicは101→111部品。CC検出、RGB_DATA pulldown、OLED connector（Rev.A footprintを流用）を追加し、U2・R3を撤去した。ERCは**35 errors / 167 warnings**（初期50 / 174）。新ブロックはnetlistと描画で確認済み。回路はまだ完成していない。
- firmware: `tusb320.c` を `usb_cc_detect.c` へ置換し、descriptorを500 mA宣言にした。一時host simulationはPASS（Rp低下の反映は17 ms）。ESP-IDF 5.3.4のRev.B構成buildは成功し、`firmware/main` のwarningは0件（2026-10-05）。実機は未実施。
- 独立レビューを2回実施した（TUSB320版／置換後）。指摘は1件を除き対応済み（回路ノート末尾）。

### 確認状況と未解決の判断事項

- **MIDI送信task（2026-10-05 対応済み）:** USB/TRSの送信taskは100 Hz設定でidle待ちが0 tickになっていた。BLEには10 msのqueue待ちがあった。3系統をtask notificationで起こす方式へ変更した。
  - producerはqueueへの積み込みかcoalesce値の更新を先に済ませてから通知する。送信taskは全部を出し切ってから待つ。この順序なので、通知を取りこぼさない。
  - USBで送信が拒否された場合（FIFO満杯・未mount）は、そのデータを保持し、必ず1 tick blockしてから再送する。通知でこの待ちを短絡させず、低優先度のTinyUSBへ実行機会を渡す。再送時は新しいPB/CC1値を古い値で上書きせず、16件ごとの挿入カウンタも維持する。
  - 切断時は従来どおり保留分を破棄する。再接続はATTACHED eventとstate taskの両方から送信taskを起こす。
  - eventの順序と、PB/CC1の集約・挿入位置（16件ごとと空になった時、TRSは8件ごと）は従来どおり。
  - host simulationで、順序、FIFO満杯時の再送、取りこぼし窓、切断・再接続、queue満杯を確認した。実機は未確認。
- **FreeRTOS tick（2026-10-05 1000 Hzへ変更済み）:** 100 Hzでは10 ms未満の待ちが0か10 msに丸められていた。そのため、matrix scanは設計の5 msではなく10 ms周期で動き、押鍵の確定は約20–30 ms（設計約10–15 ms）だった（推論）。
  - `sdkconfig.defaults`/`sdkconfig.ci` を1000 Hzにした。手元の生成済みsdkconfigも同じ行を更新した。
  - 1000 Hz未満では `matrix_scan.c` がbuildを止める。
  - 時間指定はすべて `pdMS_TO_TICKS` 経由で、ms単位の設計値は変わらない。tick割込みの負荷増と実際の押鍵遅延は実機で確認する。
- 実機でのCC応答時間、PD sourceのCC波形、Default時の総電流、MIDI送信と押鍵の実機遅延、task WDTの有無は未実施。
- ローカルの `firmware/build-revb/sdkconfig` は古い値（TRS無効、100 Hz）のままdefaultsと食い違っていたので、defaultsから再生成した。別PCでも同じ現象が起き得る。tickが古い場合は `matrix_scan.c` がbuildを止める。
- **手順3の候補比較（2026-10-05）:** 3.3 VはTPS62162 buckを第一候補、AP2112K-3.3を熱評価付き代替候補とした。VBUS保護はTVS＋過電流保護、5V_LED branchはTPS2553級のcurrent-limited switchを候補とし、USB D+/D−は低容量ESD array、CCは低漏れ保護を別条件で選ぶ。MPN・footprint・突入・実測電流は未確定。

今回のレビュー確認（2026-10-05）:

- USB再送時の新しいPB/CC1値の上書き、通知連発による再送待ちの短絡、FIFO満杯時の挿入カウンタのリセットを修正した。
- 既存の一時host harnessを修正後の実ソースに対して再実行し、順序・coalesce・FIFO満杯・通知取りこぼし窓・切断／再接続・queue満杯、および今回の再送競合と16件ごとの挿入を確認した。CC検出・configured/suspend別budgetの一時host検証もPASS。永続的なtest suiteは追加していない。
- ESP-IDF 5.3.4のRev.B buildはPASS（1000 Hz、TRS/RGB有効）。実機でのscheduler・USB転送・演奏遅延の保証ではない。
- OLED/RGB biasブロックを表題欄と重ならない位置へ移した。移動前後のnetlist接続は一致し、再描画を確認した。ERCは引き続き35 errors / 167 warnings、未割当footprintは10部品。製造不可。

確認出力: `build/hardware/20261005T072214.395967Z/`。出力はgit対象外であり、別環境では下記commandで再生成する。hash記録は依存の検出用で、別PCのlibrary同一性を自動保証しない。

## 次のAIが行うこと

1. `AGENTS.md` が示すgovernance、README、decisionsを読み、`git status --short`を確認する。未commitの成果も現在地の一部。既存変更を消さない。着手前からある `.vscode/settings.json` は今回の対象外。
2. 「確認状況」に未解決の判断事項があれば、オーナーの回答を先に反映する。監査のやり直しやRev.A全面保存には戻らない。
3. 3.3 V regulatorを選ぶ。ESP32-S3の電源要求、OLED/comparator負荷、発熱、CC VREF精度（回路ノートの誤差予算）を条件にする。あわせてVBUS入力保護（TVS、fuse/eFuse）、突入電流、CC/D±のESD、LED電源の遮断手段を具体化する。Default 500 mAの内訳は未測定項目を明示し、LED budgetだけでUSB適合を宣言しない。
4. 続いてMIDI IN/OUT、78キーmatrix、slider/button、pixel bypassを小ブロックごとにnative schematicへ追加する。型番・pin・footprintは一次資料で照合する。
5. 各ブロックでnetlist/ERC→描画→独立確認。既存PCBへ一括updateしない。機構照合では表裏・原点を揃え、0.05 mm等の差を無断で均さない。外観と衝突する新MIDI IN等は具体案を作って確認する。
6. 終了前にこの文書の状態・結果・次の一手を更新する。chat履歴、特定AIのmemory、tool session IDに依存しない。採用変更は対応する正本へ反映する。

再開依頼はこれだけでよい:

> このリポジトリのAGENTS.mdに従い、docs/rev-b-roadmap.mdの「次のAIが行うこと」から作業を続けてください。未commitの成果も引き継いでください。

同じフォルダならCodexでもClaude Codeでも使える。別PCへ移る場合は、未commitの変更を含む作業ツリーとgovernanceを渡す必要がある。リモートへpush済みとは限らない。

## 確認コマンド

リポジトリrootのPowerShellから:

```powershell
& 'C:\Program Files\KiCad\10.0\bin\python.exe' tools\verify_hardware.py
```

他の環境ではPython 3.10以上とKiCad 10.0.3を用意し、`python tools/verify_hardware.py --kicad-cli <実行ファイル>`。成果は新しい `build/hardware/<UTC>/` に出る。終了コード0は機械確認合格、1は設計違反、2は環境／tool失敗。現状の1をtool故障と誤認しない。`summary.json`、`erc.json`、`netlist.xml`、`bom.csv`、`svg/`を確認する。DRC/parity/実機は未検証として残る。

firmwareは、このPCのESP-IDF 5.3.4（`C:\Users\emnye\esp\v5.3.4`）でbuildできる。システムのPythonではexport.ps1が失敗するので、IDF同梱のPython 3.11を先にPATHへ入れる。PowerShellでリポジトリrootから:

```powershell
$env:PATH="$env:USERPROFILE\.espressif\tools\idf-python\3.11.2;$env:USERPROFILE\.espressif\tools\idf-git\2.44.0\cmd;$env:PATH"
$env:IDF_PYTHON_ENV_PATH="$env:USERPROFILE\.espressif\python_env\idf5.3_py3.11_env"
. C:\Users\emnye\esp\v5.3.4\esp-idf\export.ps1
cd firmware; idf.py -B build-revb -D SDKCONFIG=build-revb/sdkconfig -D "SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.rev-b.defaults" build
```

既存の `build-revb/sdkconfig` はdefaultsで上書きされない。defaultsを変えたら、その行を合わせるか、ファイルを退避して再生成する。

## アムニェカさんの次の作業

1. まずこの表の現在地を確認してください。外観維持、OLED（Waveshare (C)）、CC検出方式（Rd＋comparator）の回答は反映済みで、再回答は不要です。
2. 任意のAIで上の再開依頼を実行してください。次はAI側の3.3 V・保護回路の設計で、部品発注はまだ先です。firmwareのbuild確認はAIが行えます。
3. 回路と具体部品案が揃ったら、外観と新コネクタの両立案を確認してください。その後、既存 `emiuet-validation` のVAL-CORE-01手順で試作・測定へ進みます。現在のRev.B firmwareをRev.Aへそのままflashする段階ではありません。

物理測定では、Default/1.5 A/3 A、attach/detach/suspend、白色最大負荷、3.3 V温度・ripple、RGB波形を確認し、V0–V2後に製品PCBでV3へ進む。新しい並列試験体系は作らない。
