# Rev.Aおよび撤回済み判断

> **Historical / not current:** この文書は、現在のRev.Bで撤回された設計判断だけを記録します。Emiuetの現行product identityと設計判断は `../decisions.md` が正本です。

## Rev.A internal battery and PowerPath

Rev.Aはsingle-cell Li-ion battery、BQ24074 charger/PowerPath、battery NTC、BAT_VSENSE、TPS61023 5 V boostを内蔵し、充電中も動作するself-contained battery instrumentを目指していました。

Rev.Bではexternal USB 5 V onlyへ移行したため、このpower architectureとbattery validationは撤回されました。

## Rev.A dual USB

Rev.Aはcharging/power用USB-Cとdata用USB-Cを分離していました。data側にはinternal 5 VからLM66100を介したHost VBUS source pathも存在しました。

Rev.Bではsingle USB-C Device/UFPへ統合し、charging-only port、Host VBUS path、dual USB interactionを撤回しました。

## 旧Rev.B self-powered USB案

internal batteryを維持したままdata USB VBUSをpresence senseだけにするself-powered USB Device案がありました。この案ではseparate VBUS monitor GPIOとdetach handlingが必要でした。

Rev.BはUSB VBUSで本体そのものを給電するため、このself-powered monitor案を撤回しました。

## TUSB320によるCC/current検出

旧Rev.BではTUSB320（PORT=L、ADDR=L、I2C）をUFP attach/orientation/current detectorとして残していました。それ以前の「Rdだけに置き換えてTUSB320を削除する案」は、78 RGB LEDs追加後にSource current advertisementを取得する必要が生じたため一度撤回されていました。

TUSB320はCURRENT_MODE_DETECTをattach時に一度だけ更新し、接続中のRp変化はperiodic I2C soft reset（最大95 ms）と再debounce（133 ms）を経ないと反映できません（TI Rev.F §6.7、§7.3.1.2）。Type-C sinkのtSinkAdj（最大60 ms）を満たせず、reset中はRGBが周期消灯するため、2026-10-05にRd＋comparatorによる常時検出へ置換しました。TUSB320LAIもdatasheet上は同じ一回検出のため代替にしていません。

## 記録を残す理由と終了条件

これらはRev.A schematic/PCBを読む際に、現行Rev.Bとの違いを誤認しないために残します。Rev.A manufacturing dataをrepositoryから除去する判断が行われた場合、この文書もcommit historyだけへ移行できます。
