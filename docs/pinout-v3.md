# Emiuet Rev.B GPIO allocation

この表がRev.B schematicとfirmwareのGPIO正本です。ESP32-S3-MINI-1-N4R2を前提とし、
GPIO26は内蔵PSRAM用として使いません。

| Function | GPIO | Module pin | Reason |
|---|---:|---:|---|
| Slider PB / MOD / VEL | 1 / 2 / 4 | 5 / 6 / 8 | ADC1 CH0/1/3。USBやradioと競合しにくい連続群 |
| Matrix rows 0–5 | 5–10 | 9–14 | 6本連続でstring方向をescapeしやすい |
| Matrix cols 0–7 | 11–18 | 15–22 | 8本連続 |
| USB D- / D+ | 19 / 20 | 23 / 24 | native USB固定 |
| Matrix col 8 | 21 | 25 | USB pair直後の残り1本 |
| Matrix cols 9–12 | 33–36 | 28 / 29 / 31 / 32 | 4本連続。使用module variantでavailabilityを再確認 |
| TUSB320 INT_N | 37 | 33 | I2C status change、外部pull-up |
| SK6812 DATA | 38 | 34 | RMT TX → AHCT buffer |
| I2C SDA / SCL | 39 / 40 | 35 / 36 | OLEDとTUSB320の共有bus |
| SW CENTER / RIGHT | 41 / 42 | 37 / 38 | active-low button inputs |
| TRS MIDI OUT / IN | 43 / 44 | 39 / 40 | UART1 TX/RXをGPIO matrixで割当 |
| SW LEFT | 47 | 27 | active-low button input |
| Pilot LED | 48 | 30 | status output |

## Reserved and spare

| GPIO | Treatment |
|---:|---|
| 0 | BOOT/download button専用。通常機能へ共用しない |
| 3 | strapping。spare test pad候補だがreset中は外部駆動禁止 |
| 26 | N4R2 embedded PSRAM。使用禁止 |
| 45 / 46 | strapping。spare test pad候補だが量産機能へ割当しない |

GPIO3/45/46は「空き」ではありますが、安全な汎用spareとは扱いません。GPIO0を含む
4本はboot/recoveryを優先します。GPIO19/20にはLED、UART、matrixを重ねません。

## Routing intent

行列は5–18、21、33–36へまとめ、MCUからkey fieldへ束として出します。USB pairは
19/20からconnectorへ最短・等長で配線し、matrixから離します。ADC1の1/2/4は
slider RC filterへ短く配線します。RGB dataは38からbuffer、series resistor、D1へ
進み、5V power returnとは平行長距離にしません。UART43/44はTRS edge側へ一組で出します。

旧GPIO表、BAT_VSENSE、CHG、PGOOD、GPIO42 VBUS monitorはRev.A/撤回案です。
