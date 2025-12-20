# Emiuet Pin Assignment (Final) v3.0

このドキュメントは **Emiuet ファームウェアおよび PCB 設計における唯一の正（Single Source of Truth）** として使用される最終確定ピンアサイン資料です。

* 本資料の内容は、README / design-notes / decisions よりも **優先度が高い** ものとします
* ファームウェア実装時は、本資料を直接参照して GPIO 定義を行ってください
* PCB Rev.3 以降では、本資料を前提として差し替えを行って問題ありません

---

## 1. システム・通信・電源系

ESP32-S3-MINI-1 の安定動作に必須の固定機能ピンです。

| 機能         | GPIO   | 物理 Pin | 備考                         |
| ---------- | ------ | ------ | -------------------------- |
| USB D-     | GPIO19 | Pin 23 | USB 差動ペア必須                 |
| USB D+     | GPIO20 | Pin 24 | USB 差動ペア必須                 |
| MIDI OUT   | GPIO43 | Pin 39 | TRS Type-A / U0TXD         |
| I2C SDA    | GPIO18 | Pin 22 | OLED / TUSB320 共有（3.3V PU） |
| I2C SCL    | GPIO16 | Pin 20 | OLED / TUSB320 共有          |
| BAT_VSENSE | GPIO17 | Pin 21 | バッテリー電圧監視（ADC2_CH6）        |
| EN         | -      | Pin 45 | 10kΩ + 1μF RC 遅延必須         |
| CHG        | GPIO48 | Pin 30 | 充電中ステータス（10kΩ 外部 PU）       |
| PGOOD      | GPIO38 | Pin 34 | 外部電源検知（10kΩ 外部 PU）         |

---

## 2. アナログ入力（Slider）・物理 UI

演奏操作および状態表示に関わる入力系です。

| 機能             | GPIO   | 物理 Pin | 備考                     |
| -------------- | ------ | ------ | ---------------------- |
| Slider-1 (PB)  | GPIO1  | Pin 5  | ピッチベンド（上方向専用）          |
| Slider-2 (Mod) | GPIO2  | Pin 6  | モジュレーション（CC#1）         |
| Slider-3 (Vel) | GPIO4  | Pin 8  | ベロシティ（NoteOn 時に反映）     |
| LED（統合）        | GPIO6  | Pin 10 | 単色 LED（状態 / 充電表示）      |
| SW_CENTER      | GPIO40 | Pin 36 | MPE 切替 / 長押し BLE ペアリング |
| SW_RIGHT       | GPIO39 | Pin 35 | オクターブ Up 等             |
| SW_LEFT        | GPIO44 | Pin 40 | オクターブ Down（U0RXD 活用）   |

---

## 3. キーマトリクス（6 行 × 13 列 / 78 Keys）

最新の PCB 修正を反映したマッピングです。

### 3.1 弦（Row / 駆動側）

| 弦    | GPIO   | 物理 Pin |
| ---- | ------ | ------ |
| Str1 | GPIO5  | Pin 9  |
| Str2 | GPIO7  | Pin 11 |
| Str3 | GPIO8  | Pin 12 |
| Str4 | GPIO9  | Pin 13 |
| Str5 | GPIO11 | Pin 15 |
| Str6 | GPIO10 | Pin 14 |

### 3.2 フレット（Column / 検出側）

| フレット  | GPIO   | 物理 Pin |
| ----- | ------ | ------ |
| Frt0  | GPIO46 | Pin 44 |
| Frt1  | GPIO45 | Pin 41 |
| Frt2  | GPIO35 | Pin 31 |
| Frt3  | GPIO36 | Pin 32 |
| Frt4  | GPIO37 | Pin 33 |
| Frt5  | GPIO34 | Pin 29 |
| Frt6  | GPIO33 | Pin 28 |
| Frt7  | GPIO47 | Pin 27 |
| Frt8  | GPIO21 | Pin 25 |
| Frt9  | GPIO15 | Pin 19 |
| Frt10 | GPIO14 | Pin 18 |
| Frt11 | GPIO13 | Pin 17 |
| Frt12 | GPIO12 | Pin 16 |

---

## 4. 基板設計・実装上の重大な留意事項

### 4.1 Strapping Pin（GPIO45 / GPIO46）の入力利用

* GPIO45（Frt1）および GPIO46（Frt0）は **起動モード決定用 Strapping Pin**
* 起動時にこれらのキーが押されていると、以下の問題が発生する可能性あり

  * 書き込み不可
  * 正常起動しない

**対策（必須）**

* ファームウェア側で **起動完了後にキーマトリクス走査を開始**する
* 起動直後の初回スキャン入力は無効化すること
* 内部プルダウンがデフォルトである点を考慮する

---

### 4.2 PSRAM 専用ピンの競合回避

* GPIO26（Pin 26）は ESP32-S3-MINI-1 内部で PSRAM に使用
* 本設計では **完全に未使用**
* メモリ競合リスクは回避済み

---

### 4.3 アナログ信号のノイズ対策

* スライダー入力および BAT_VSENSE はノイズの影響を受けやすい
* 以下の信号との長距離並走は値のフラつきを引き起こす可能性あり

  * GPIO6（LED / PWM）
  * キーマトリクス Row 駆動線

**推奨対策**

* AGND（アナログ GND）ゾーニングの徹底
* MCU 直近に RC フィルタを配置

  * 例: 100Ω + 0.1μF

---

### 4.4 オクターブボタン（SW_LEFT）の復活

* GPIO44（Pin 40）を Frt0 から分離し、SW_LEFT に割り当て
* オクターブ Down 等の物理操作系を安定確保
* Pin 44 / 41 / 40 が物理的に隣接しているため、**パターンショートに注意**

---

## 5. 本ドキュメントの位置づけ

* 本資料は **Emiuet v3 系の最終ピンアサイン定義**である
* ファームウェア実装時は、GPIO 定義・初期化・安全対策を必ず本資料に準拠すること
* 今後 PCB Revision が変わる場合は、必ず本ファイルを更新し、バージョンを明示すること

---

*End of document.*
