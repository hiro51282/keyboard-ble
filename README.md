# k400plus-fn-remapper（失敗プロジェクト）

## 概要

`k400plus-fn-remapper` は、Logitech K400 Plus キーボードのファンクションキー問題をハードウェアで解決しようとしたプロジェクトです。

ESP32とCH9350を用いて、Unifyingレシーバの入力をBLEキーボードとして再送しつつ、Fキーの挙動を補正することを目的としていました。

**結論として、このアプローチでは完全な解決は不可能であると判断し、プロジェクトは中止しました。**

---

## なぜ失敗したのか

### 1. 情報の不可逆変換

K400の一部のFキー（例: F6）は、デバイス内部で既に以下のように変換されています：

```
F6 → Win + D
```

この時点で

```
・「F6が押された」という情報は消失
・「Win+Dが押された」という状態だけが残る
```

そのため、後段（CH9350 / ESP32）では

```
F6 と 手入力の Win+D を区別することは原理的に不可能
```

---

### 2. タイミングによる識別の限界

以下のような試行を行いました：

* idleフレーム（0x82）による時間差判定
* 状態遷移の監視
* エッジ検出
* pending制御

しかし

```
・メディアキーと手入力の時間差はほぼ同一
・フレーム構造も同一
```

結果として

```
「安定して識別する条件」が存在しない
```

---

### 3. 実装の崩壊

識別を試みた結果、コードは以下の状態に陥りました：

* 状態フラグの増殖（measuring / idleCount / pending など）
* 時間依存ロジックの複雑化
* イベントと状態の責務混在
* 可読性の著しい低下

これは

```
「解けない問題を無理に解こうとした結果」
```

であり、設計として破綻しています。

---

## 現在の結論

```
・K400のFキー問題はソフトウェア後段では解決できない
・解決にはデバイス設定（Solaar等）または別ハードが必要
```

---

## 成果（残せるもの）

本プロジェクトには以下の技術的成果があります：

* CH9350のフレーム解析
* USB→UART→BLEのパイプライン構築
* BLE HID（KeyReport）送信実装
* 6KRO状態同期の理解

これらは別プロジェクトに転用可能です。

---

## 今後の方針

本リポジトリは以下の用途で扱います：

* 失敗事例の記録
* CH9350プロトコル解析の参考
* BLEキーボード実装のベースコード

---

## ステータス

```
✖ Fキー問題の完全解決（不可能）
✔ Unifying → BLE変換
✔ フレーム解析
✔ 基本キーボード動作
```

---

## 教訓

```
・情報が欠損した後では復元できない
・状態や時間で補完しようとすると設計が崩壊する
・解けない問題は早めに見切るべき
```

---

## 作者メモ

「これは技術不足じゃなくて仕様の壁。」

---

## CH9350 生データ取得コード（ログ用）

```cpp
#include <Arduino.h>

HardwareSerial mySerial(2);

void setup()
{
  Serial.begin(115200);
  mySerial.begin(115200, SERIAL_8N1, 16, 17);
}

void loop()
{
  static uint8_t buf[64];
  static int idx = 0;

  while (mySerial.available())
  {
    uint8_t d = mySerial.read();

    if (d == 0x57)
    {
      if (idx > 0)
      {
        Serial.print("FRM: ");
        for (int i = 0; i < idx; i++)
        {
          if (buf[i] < 0x10) Serial.print("0");
          Serial.print(buf[i], HEX);
          Serial.print(" ");
        }
        Serial.println();
      }
      idx = 0;
    }

    if (idx < (int)sizeof(buf))
    {
      buf[idx++] = d;
    }

    if (idx >= 32)
    {
      Serial.print("OVF: ");
      for (int i = 0; i < idx; i++)
      {
        if (buf[i] < 0x10) Serial.print("0");
        Serial.print(buf[i], HEX);
        Serial.print(" ");
      }
      Serial.println();
      idx = 0;
    }
  }
}
```
