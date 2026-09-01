# AI作業指示

このリポジトリで作業するAIは、着手前に次を全文確認する。

* `../eub/ai-governance/AI-WORKING-CHARTER.md`
* `../eub/ai-governance/AI-WORKING-POLICY.md`
* このリポジトリの `README.md` と `docs/decisions.md`

相対パスの `eub` が利用できない環境では、`https://github.com/emnyeca/eub/tree/main/ai-governance` の正本を確認する。確認できないまま、製品方針や正本構造を変更してはならない。

## Emiuet固有の確認事項

* Emiuetの中心は、ギター由来の6×13音配列を持つ表現的なMIDIコントローラーである。TYPEは補助機能であり、hardware revisionや直近の実装を製品identityより優先しない。
* `docs/decisions.md` は現在有効な製品・設計判断の正本である。旧revisionで決められた事項でも、有効な判断を一括してhistoryへ移さない。
* 製品ユーザー向け文書は英語、開発者・AI向け内部文書は日本語を原則とする。変更前に各文書の読者を分類する。
* 標準MIDIメッセージをdevice設定へ転用しない。未定義の制御仕様を実装で先に確定しない。

完了前に、目的と主従、文書言語、current/history、正本への導線、実装と文書の一致を確認する。
