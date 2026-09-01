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

## TUSB320削除案

fixed UFPならCC1/CC2へRdを直接配置できるため、TUSB320を削除する案がありました。78 RGB LEDs追加後、Source current advertisementをfirmwareで取得する価値が高くなったため撤回しました。

現行Rev.BではTUSB320をUFP attach/orientation/current detectorとして残します。Host、DRP、role switchingには使用しません。

## 記録を残す理由と終了条件

これらはRev.A schematic/PCBを読む際に、現行Rev.Bとの違いを誤認しないために残します。Rev.A manufacturing dataをrepositoryから除去する判断が行われた場合、この文書もcommit historyだけへ移行できます。
