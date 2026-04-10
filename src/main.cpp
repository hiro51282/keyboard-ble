// =============================================
// ルール
// ① いきなり変更しない。必ず相談すること
// ② 将来的に責務分割するため、ファイル名を見出し1にする
// ③ 直接貼り付けして実行できる状態を維持する
// =============================================

// # main.cpp

#include <Arduino.h>
#include <BleKeyboard.h>

HardwareSerial mySerial(2);
BleKeyboard bleKeyboard("K400Remapper", "ESP32", 100);

uint8_t buf[8];
int idx = 0;

// 直前状態（無駄送信防止）
uint8_t prev_m2 = 0;
uint8_t prev_k2 = 0;

// ===== setup =====
void setup()
{
  Serial.begin(115200);
  mySerial.begin(115200, SERIAL_8N1, 16, 17);
  bleKeyboard.begin();
}

// ===== loop（受信 + KeyReport送信） =====
void loop()
{
  while (mySerial.available())
  {
    uint8_t d = mySerial.read();

    if (d == 0x57)
      idx = 0;

    if (idx < 8)
      buf[idx++] = d;

    if (idx == 8)
    {
      uint8_t m1 = buf[4];
      uint8_t m2 = buf[5];
      uint8_t k1 = buf[6];
      uint8_t k2 = buf[7];

      // ===== 生データログ =====
      Serial.printf("m1=0x%02X m2=0x%02X k1=0x%02X k2=0x%02X\n", m1, m2, k1, k2);

      // ===== 状態変化があった時だけ送信 =====
      if (m2 != prev_m2 || k2 != prev_k2)
      {
        if (bleKeyboard.isConnected())
        {
          KeyReport report = {0};

          // 修飾子は常に現在状態を反映
          report.modifiers = m2;

          // k2が0なら「キーなし」= release
          report.keys[0] = k2; 

          bleKeyboard.sendReport(&report);
        }

        prev_m2 = m2;
        prev_k2 = k2;
      }

      idx = 0;
    }
  }
}
