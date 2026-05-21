# CH9350 → ESP32 → BLE Keyboard + Mouse Bridge

## 概要

有線 USB キーボード / マウスを CH9350 で UART 化し、
ESP32 で BLE HID（Keyboard + Mouse）として PC に接続するプロジェクト。

目的は

* 普段使い（主にコーディング用途）の無線入力環境構築
* ゲーミング用途ではなく、安定した日常利用

です。

「最速」ではなく、
**ちゃんと使えること** を優先しています。

---

## BLE Stack / Library

本プロジェクトは ESP32 のデフォルト BLE（Bluedroid）ではなく、

**NimBLE（NimBLE-Arduino）ベースのスタック**を使用しています。

さらに、以下のライブラリを内部に取り込み（vendor化）して使用しています：

```text
lib/ESP32-NimBLE-Combo
```

理由：

* notify 詰まりが少ない
* 軽量で安定性が高い
* BLE HID用途での挙動が良好

※ Bluedroid では同条件でも安定しない可能性があります

---

## 現在の構成

```text
USB Keyboard / Mouse
        ↓
      CH9350
        ↓ UART
      ESP32
        ↓ BLE HID
        PC
```

---

## 動作状況

### Keyboard

* BLE Keyboard 動作確認済み
* 日本語配列で実用可能
* modifier 対応（Shift / Ctrl など）
* 同時押し（6KRO）対応

### Mouse

* move 動作確認済み
* wheel（縦スクロール）対応
* Left Click
* Right Click
* Middle Click
* Back
* Forward

### 未対応

* チルトホイール（横スクロール）

  * 使用頻度が低いため未実装

---

## CH9350 設定

### DIPスイッチ

今回使用している構成（状態2 + baud 300000）は以下です。

```text
SEL
BAUD1
BAUD0
S1
S0

1
0
0
1
0
```

つまり

```text
SEL=1 （下位機）
BAUD1=0
BAUD0=0
S1=1
S0=0
```

です。

※ CH9350 は資料差分や中華実装差があるため、
手元個体での実測を優先しています。

---

### 役割

* CH9350 が USB Host として Keyboard / Mouse を受ける
* 状態2モードで UART 出力
* ESP32 側で BLE HID に再構成

---

### モード

* 状態2モード

### UART

* baud = 300000

### フレーム種別

```text
0x01 → Keyboard
0x02 → Mouse
0x80 → keepalive / status（無視）
```

---

## Wiring

```text
CH9350      ESP32
-------------------------
TX      →   RX (GPIO16)
RX      →   TX (GPIO17)
VCC     →   5V
GND     →   GND
```

※ 本プロジェクトでは VCC は **5V 駆動** としています（使用モジュール前提）

---

## Keyboard frame

```text
57 AB 01 [modifier] [reserved] [key1] [key2] [key3] [key4] [key5] [key6]
```

### 実装

* `BleComboKeyboard`
* `BleComboMouse`
* `BleCombo` を内包した構成を使用
* `sendReport()` を使用
* `press()` / `release()` は未使用

### 日本語配列対応（重要）

`BleCombo` の HID レポート定義を修正しています。

```text
LOGICAL_MAXIMUM(1), 0xFF   // 256 keys
USAGE_MAXIMUM(1),   0xFF
```

これにより

* 日本語配列の安定動作
* usage 範囲拡張
* 一部キーの欠落防止

が可能になっています。

標準のままだと usage 範囲不足により
一部キーが正しく扱えませんでした。

### index

```text
modifier = f.data[3]
keys     = f.data[5] ～ f.data[10]
```

---

## Mouse frame

```text
57 AB 02 [button] [dx] [dy] [wheel]
```

### 実装

* `BleComboMouse`
* move + wheel
* button press / release

### 安定化

BLE notify 詰まり対策として
mouse は周期送信しています。

```cpp
constexpr unsigned long SEND_INTERVAL_US = 7500;
```

現在の最適値は **7.5ms（7500µs）** です。

テスト・実運用を通じて最適化した値となっています。

---

## 重要だったこと

### 犯人は readRawFrame() だった

以前の実装は

* `0x88` 前提
* `len + 4`
* `type == 0x82` 特殊処理

など、旧仕様の亡霊を引きずっていました。

状態2モードでは

```text
0x01 / 0x02 / 0x80
```

だけをシンプルに扱うのが正解でした。

変に賢い parser より、
雑で正しい parser の方が強いです。

---

## Known Issues

* マウス move は完全にネイティブレベルではない（ただし日常利用は可能）
* ゲーミング用途には向かない
* BLE のため低遅延には限界あり
* 複数 PC の切り替えは今後の課題

---

## 今後やるかもしれないこと

* 複数 PC 切り替え
* bond 管理改善

ただし、
今は「ちゃんと使える」ので
無理に地獄へ進まない予定です。

---

## 結論

動くなら勝ち。

かなり遠回りしましたが、
最終的に

**ちゃんと使える BLE Keyboard + Mouse**

になりました。

Kanpeki☆

---

## 複数PC切り替え機能

### 実装済み機能

2台のPC間でキーボード・マウス入力を切り替えられる機能を実装済み。

* **ScrollLock 短押し**: 次のbond済みPCへ切り替え（`switchTarget()`）
* **ScrollLock 3秒長押し**: 全bond削除・ペアリングモードへ（`enterPairingMode()`）
* **GPIO23 短押し**: 同上（物理ボタン実装時）
* **GPIO23 3秒長押し**: 同上（物理ボタン実装時）

### 切り替えフロー

```
ScrollLock 短押し（or GPIO23 短押し）
  ↓
advertiseOnDisconnect 一時無効化
  ↓
現在のPCを disconnect
  ↓
bond リストから「次のPC」アドレスを取得
  ↓
whitelist に次のPCのみを追加 → whitelist filter でアドバタイズ
（前のPCからの意図しない再接続を防ぐ）
  ↓
次のPCが自動接続（数秒） → onConnect でwhitelistをリセット
```

### 初回ペアリング手順

1. 両PCでこのデバイスのBLE接続を削除（「デバイスを削除」）
2. ESP32をリセット（または後述の NVS erase を実施）
3. ScrollLock を 3 秒長押し → ペアリングモードへ（全bond削除、アドバタイズ開始）
4. 1台目のPCでペアリング → 接続・認証完了を確認（シリアルで `bonds=1` を確認）
5. 1台目のPCのBTを一時オフ（切断、2台目がつなぎやすくする）
6. 2台目のPCでペアリング → 接続・認証完了を確認（シリアルで `bonds=2` を確認）
7. ScrollLock 短押しで切り替えを確認

---

### 根本原因の調査記録：bond=1 問題

実装後、bond 数が常に 1 になり `switchTarget()` が機能しない問題が発生した。

#### 症状

```
AuthComplete: addr=<Win>   bonded=no bonds=0  ← Win ペアリング直後
AuthComplete: addr=<Linux> bonded=no bonds=1  ← Linux ペアリング後もbonds=1のまま
Switch: 1 bonded device(s)                    ← 切り替え不能
```

さらに `bond[0]` が Win / Linux で交互に切り替わる（eviction の証拠）。

#### 根本原因：CCCD テーブルオーバーフロー

NimBLE は通知サブスクリプション状態（CCCD: Client Characteristic Configuration Descriptor）を NVS に永続保存する。本プロジェクトの BLE HID デバイスには 5 つの NOTIFY 特性がある:

| 特性 | UUID |
|---|---|
| Keyboard input report | 0x2A4D |
| Media keys input report | 0x2A4D |
| Mouse input report | 0x2A4D |
| Battery level | 0x2A19 |
| Boot keyboard input | 0x2A22 |

PCが接続すると各 NOTIFY 特性に対して CCCD エントリが作成される。

| PC台数 | 必要 CCCD エントリ数 |
|---|---|
| 1台 | 5 × 1 = 5 |
| **2台** | 5 × 2 = **10** |
| 3台 | 5 × 3 = 15 |

NimBLE-Arduino のデフォルト値（`syscfg.h:929`）:

```c
#define MYNEWT_VAL_BLE_STORE_MAX_CCCDS (8)  // 2台で不足!
```

#### eviction の仕組み

1. Win がペアリング → CCCD 5件保存（合計 5）
2. Win 切断 → bond 保存（bonds=1）
3. Linux が接続し通知をサブスクライブ → 9件目の CCCD 書き込みで `BLE_HS_ESTORE_CAP` 返却
4. `ble_store_util_status_rr`（NimBLE のデフォルト store callback）が `BLE_STORE_EVENT_OVERFLOW` を受信
5. CCCD overflow の場合 `ble_gap_unpair_oldest_except(Linux_addr)` を呼ぶ（`ble_store_util.c`）
6. **Win の bond が bond + CCCD ごと削除される**
7. Linux の bond 保存 → bonds=1（Linux のみ）

`ble_store_util_status_rr` はサンプルアプリ向けの実装でありプロダクト向けではないが、NimBLE-Arduino では `NimBLEDevice.cpp:898` でデフォルトとして設定されている。

#### 修正内容

`platformio.ini` の `build_flags` に以下を追加:

```ini
-DMYNEWT_VAL_BLE_STORE_MAX_CCCDS=15
```

`syscfg.h` の `#ifndef MYNEWT_VAL_BLE_STORE_MAX_CCCDS` ガードをコンパイル前に上書きし、配列サイズと容量チェックに 15 が使われるようになる（5特性 × 3台 = 15）。

#### NVS erase が必要な理由

過去のペアリング試行で NVS に不正な状態（CCCD エントリが 8 件詰まった状態）が残っている可能性がある。次回フラッシュ時に一度 NVS を消去することで、クリーンな状態からペアリングを開始できる。

```bash
pio run -e esp32dev -t erase   # フラッシュ全体を消去
pio run -e esp32dev -t upload  # 再フラッシュ
```

### ロールバック方法

変更ファイルは `platformio.ini` のみ（`build_flags` 追加）。

`git revert <commit>` または `build_flags` から削除するだけで元に戻せる。  
NVS の bond 情報は `NimBLEDevice::deleteAllBonds()`（ScrollLock 3秒長押し）または各PCの「ペアリング解除」で削除可能。
