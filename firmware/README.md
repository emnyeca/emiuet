# Emiuet Rev.B firmware

ESP-IDF 5.3.4、ESP32-S3-MINI-1-N4R2向けです。

## Data paths

```text
matrix → input_router → USB/TRS MIDI TX or USB HID
USB MIDI RX / isolated TRS MIDI RX → midi_input → led_control
→ led_renderer → espressif/led_strip RMT DMA → SK6812 x78
CC comparator (GPIO37) → usb_cc_detect → usb_power → renderer current limiter / OLED status
```

USBはbus-powered Composite Deviceです。self-powered VBUS GPIO、Host stack、role
negotiation、battery/charger stateはありません。

初期RGB inputはMIDI Note On/OffとCC123 All Notes Offです。global brightnessを含む
device-level LED control protocolは未確定です。CC7などのstandard musical CCを
device configurationへ転用せず、将来SysEx等の明示的なprotocolとして定義します。
SysEx拡張点はtransportの外側に残していますが、protocolはまだ固定しません。

## Build

ESP-IDF shellで次を実行します。

```text
idf.py -B build-revb -D SDKCONFIG=build-revb/sdkconfig \
  -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.rev-b.defaults" build
```

FreeRTOS tickは1000 Hz（`CONFIG_FREERTOS_HZ=1000`）を前提にしています。100 Hzでは `pdMS_TO_TICKS(1)` と `pdMS_TO_TICKS(5)` が0になり、matrix scanは最小1 tick補正で10 ms周期、補正のない再送待ちは待機なしになっていたためです。既存のbuild用sdkconfigはdefaultsで上書きされないので、古い値のままだと `matrix_scan.c` がbuildを止めます。その場合はbuild directoryの `sdkconfig` の該当行を直すか、`sdkconfig` を退避して再生成してください。

MIDI送信task（USB/TRS/BLE）は、producerからのtask notificationで起きて、queueとcoalesce値を出し切ってから待ちます。演奏側は待ちなしでenqueueし、通知するだけです。

`espressif/led_strip`はRMT backendとESP32-S3 DMAを使用します。`sdkconfig.defaults`の
Default/1.5A LED budgetとbrightness ceilingはVAL-CORE-01/V3実測後に確定します。

Host-side logic testsは `tests/` にあります。ESP-IDF buildがUSB descriptor、I2C、
UART、RMTを含むintegration checkです。

## Rev.B設計中の電力制御

起動直後のCC判定前はRGB budgetを0にします。Default広告ではUSB configuredかつ非suspendの場合だけ200 mA設定を許可し、Rp ≥ 1.5 A（3 Aを含む）では1000 mA設定を上限とします。これはLEDの推定予算であり、基板全体のUSB消費電流保証ではありません。

`usb_cc_detect` は10 ms周期でGPIO37を読み、Rp低下は2 sample、上昇は100 ms安定で反映します。Type-C tSinkAdj（60 ms）内に収める設計ですが、実機の応答時間測定は未完了です。根拠・次の作業は [回路ノート](../docs/rev-b-circuit-notes.md) と [ロードマップ](../docs/rev-b-roadmap.md) を参照してください。
