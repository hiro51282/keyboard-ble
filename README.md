# CH9350 → ESP32 → BLE Keyboard + Mouse Bridge

## 概要

有線 USB キーボード / マウスを CH9350 で UART 化し、
ESP32 で BLE HID（Keyboard + Mouse）として PC に接続するプロジェクト。

目的は

* モニターアーム導入による配線削減
* 普段使い（主にコーディング用途）の無線入力環境構築
* ゲーミング用途ではなく、安定した日常利用

です。

「最速」ではなく、
**ちゃんと使えること** を優先しています。

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

### 日本語配列対応

`BleCombo` 側の `MAX_USAGE` を拡張しています。

これにより

* 日本語配列の安定動作
* `\`（バックスラッシュ）キー入力

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
constexpr unsigned long SEND_INTERVAL_US = 36000;
```

速くしすぎると notify エラーが増え、
逆に操作感が悪化しました。

この値が実用上かなり安定しています。

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

* マウス move は快適ではない（が実用可能）
* ゲーミング用途には向かない
* BLE のため低遅延には限界あり
* 複数 PC の切り替えは今後の課題

---

## 今後やるかもしれないこと

* 複数 PC 切り替え
* bond 管理改善
* NimBLE 直実装
* ESP-IDF 移行

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
