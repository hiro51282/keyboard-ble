# k400plus-fn-remapper

## 概要

`k400plus-fn-remapper` は、Logitech K400 Plus キーボードのファンクションキー問題をハードウェアで解決するプロジェクト。

PC側にソフトウェアを入れず、外部デバイスでキー変換を行う。

---

## コンセプト

USB処理は専用ICにオフロードし、ESP32はシリアル受信と変換処理に専念する。

```
K400 Plus
  ↓ (2.4GHz)
Unifying Receiver
  ↓ (USB)
CH9350（USB→UART変換）
  ↓ (UART)
ESP32（DevKitC）
  ↓ (BLE HID)
PC
```

---

## ハード構成

### 使用デバイス

* ESP32 DevKitC
* CH9350 モジュール
* Unifying Receiver

---

## 配線

```
CH9350 → ESP32

5V   → 5V（またはVIN）
GND  → GND
TX   → RX（例: GPIO16）
RX   → TX（例: GPIO17）
```

* TX/RXはクロス接続
* RS485ピンは未使用

---

## DIPスイッチ設定

| スイッチ    | 役割           |
| ------- | ------------ |
| S0/S1   | モード設定        |
| BA0/BA1 | ボーレート        |
| SEL     | UART / 485切替 |

### 推奨設定

* UARTモード
* ボーレート: 9600 or 115200

※詳細は基板裏面シルク参照

---

## ソフト構成

### ESP32側

* UARTでデータ受信
* HIDコード解析
* キー変換
* BLE送信

---

## 開発環境

* PlatformIO（Arduino framework）

👉 シンプル・再現性重視

---

## 最小動作確認コード

```cpp
#include <Arduino.h>

HardwareSerial mySerial(2);

void setup() {
    Serial.begin(115200);
    mySerial.begin(9600, SERIAL_8N1, 16, 17);
}

void loop() {
    while (mySerial.available()) {
        int data = mySerial.read();
        Serial.print(data);
        Serial.print(" ");
    }
}
```

---

## 目的

* K400 PlusのFキー問題解決
* ソフト不要
* 安定動作
* シンプル構成

---

## キー対応メモ

### K400 メディアキー → Linuxキーコード

| Fキー | 機能           | Linuxキーコード                        | CH9350生データ（例）                              | KEYDOWN/UPメモ                |
| --- | ------------ | --------------------------------- | ------------------------------------------ | --------------------------- |
| F1  | 戻る           | KEY_BACK (158)                    | 57 AB 88 8 31 3 24 2 / 57 AB 88 8 31 3 0 0 | Down: 24 2 → Up: 0 0        |
| F2  | ホーム          | KEY_HOMEPAGE (172)                | 57 AB 88 8 31 3 23 2 / 57 AB 88 8 31 3 0 0 | Down: 23 2 → Up: 0 0        |
| F3  | アプリケーション切替   | KEY_LEFTALT (56) + KEY_TAB (15)   | 57 AB 88 B 11 4 0 2B / 57 AB 88 B 11 4 0 0 | Down: 2B → Up: 0 0          |
| F4  | メニュー         | KEY_COMPOSE (127)                 | 57 AB 88 B 11 0 0 65 / 57 AB 88 B 11 0 0 0 | Down: 65 → Up: 0 0          |
| F5  | 検索           | KEY_SEARCH (217)                  | 57 AB 88 8 31 3 21 2 / 57 AB 88 8 31 3 0 0 | Down: 21 2 → Up: 0 0        |
| F6  | デスクトップ表示/非表示 | KEY_LEFTMETA (125) + KEY_D (32)   | 57 AB 88 B 11 8 0 7 など                     | Win↓→D↓→D↑→Win↑             |
| F7  | ウィンドウ最大化     | KEY_LEFTMETA (125) + KEY_UP (103) | 57 AB 88 B 11 8 0 52 / 57 AB 88 B 11 8 0 0 | Down: 52 → Up: 0 0（Win修飾あり） |
| F8  | 画面切り替え       | KEY_LEFTMETA (125) + KEY_P (25)   | 57 AB 88 B 11 8 0 13 / 57 AB 88 B 11 8 0 0 | Down: 13 → Up: 0 0（Win修飾あり） |
| F9  | メディア         | KEY_CONFIG (171)                  | 57 AB 88 8 31 3 83 1 / 57 AB 88 8 31 3 0 0 | Down: 83 1 → Up: 0 0        |
| F10 | 前のトラック       | KEY_PREVIOUSSONG (165)            | 57 AB 88 8 31 3 B6 0 / 57 AB 88 8 31 3 0 0 | Down: B6 0 → Up: 0 0        |
| F11 | 再生/一時停止      | KEY_PLAYPAUSE (164)               | 57 AB 88 8 31 3 CD 0 / 57 AB 88 8 31 3 0 0 | Down: CD 0 → Up: 0 0        |
| F12 | 次のトラック       | KEY_NEXTSONG (163)                | 57 AB 88 8 31 3 B5 0 / 57 AB 88 8 31 3 0 0 | Down: B5 0 → Up: 0 0        |

※ evtestにて取得

---

## 修飾キー（Modifier）マッピング

| キー       | m1   | m2   | 備考                  |
| -------- | ---- | ---- | ------------------- |
| Shift（左） | 0x11 | 0x02 | 押下で2、離すと0           |
| Shift（右） | 0x11 | 0x20 |                     |
| Ctrl（左）  | 0x11 | 0x01 |                     |
| Ctrl（右）  | 0x11 | 0x10 |                     |
| Alt（左）   | 0x11 | 0x04 |                     |
| Alt（右）   | 0x11 | 0x40 |                     |
| Win      | 0x11 | 0x08 |                     |
| Fn       | -    | -    | CH9350に出力されない（内部処理） |

※ KEYDOWN: m2にビットが立つ
※ KEYUP: m2 = 0x00

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

    // フレーム開始検出
    if (d == 0x57)
    {
      idx = 0;
    }

    buf[idx++] = d;

    // 最低限の長さで表示
    if (idx >= 8)
    {
      for (int i = 0; i < idx; i++)
      {
        Serial.print(buf[i], HEX);
        Serial.print(" ");
      }
      Serial.println();
      idx = 0;
    }
  }
}
```

👉 用途

* CH9350の生フレーム確認
* キー押下/解放の流れ解析
* マッピング作成のためのログ取得

---

## 今後の実装

* [ ] HIDコード解析
* [ ] メディアキー → Fキー変換
* [ ] BLE HID送信
* [ ] キーマップ設計

---

## 非目的

* USB Hostを自前実装すること

---

## ステータス

🚧 配線確認・UART受信確認フェーズ

---

## ライセンス

未定
