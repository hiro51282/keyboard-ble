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

// ===== 構造体（送信用フォーマット） =====
struct KeyState
{
  uint8_t modifiers;
  uint8_t keys[6];
};

// 特殊フレーム（Fキー等の単発イベントを保持）
struct SpecialFrame
{
  uint8_t raw[16];
  uint8_t length;
};

// ===== パーサ（可変長フレーム → 構造体） =====
// 戻り値：0=なし / 1=KeyState / 2=SpecialFrame
int readFrame(KeyState &state, SpecialFrame &special)
{
  static uint8_t buf[64];
  static int idx = 0;

  while (mySerial.available())
  {
    uint8_t d = mySerial.read();

    // フレーム開始
    if (d == 0x57)
    {
      idx = 0;
    }

    if (idx < (int)sizeof(buf))
    {
      buf[idx++] = d;
    }

    // 最低ヘッダ
    if (idx < 4)
      continue;

    // ヘッダ確認
    if (buf[0] != 0x57 || buf[1] != 0xAB)
      continue;

    uint8_t type = buf[2];
    uint8_t len  = buf[3];

    // フレーム完成待ち
    if (idx < (len + 4))
      continue;

    // ハートビート無視
    if (type == 0x82)
    {
      idx = 0;
      return 0;
    }

    // キーイベント
    if (type == 0x88)
    {
      // ===== 状態フレーム =====
      if (len >= 0x0B)
      {
        state.modifiers = buf[5];

        for (int i = 0; i < 6; i++)
        {
          state.keys[i] = buf[7 + i];
        }

        idx = 0;
        return 1;
      }
      // ===== 特殊フレーム =====
      else
      {
        special.length = (len + 4 < sizeof(special.raw)) ? (len + 4) : sizeof(special.raw);

        for (int i = 0; i < special.length; i++)
        {
          special.raw[i] = buf[i];
        }

        idx = 0;
        return 2;
      }
    }

    idx = 0;
  }

  return 0;
}

// ===== setup =====
void setup()
{
  Serial.begin(115200);
  mySerial.begin(115200, SERIAL_8N1, 16, 17);
  bleKeyboard.begin();
}

// ===== loop（構造化取得モード + BLE送信） =====
void loop()
{
  KeyState state = {0};
  SpecialFrame special = {0};

  int result = readFrame(state, special);

  if (result == 1)
  {
    // ===== デバッグ表示 =====
    Serial.print("STATE: mod=");
    Serial.print(state.modifiers, HEX);
    Serial.print(" keys=");

    for (int i = 0; i < 6; i++)
    {
      if (state.keys[i] < 0x10) Serial.print("0");
      Serial.print(state.keys[i], HEX);
      Serial.print(" ");
    }
    Serial.println();

    // ===== BLE送信（そのまま反映） =====
    if (bleKeyboard.isConnected())
    {
      KeyReport report = {0};

      report.modifiers = state.modifiers;

      for (int i = 0; i < 6; i++)
      {
        report.keys[i] = state.keys[i];
      }

      bleKeyboard.sendReport(&report);
    }
  }
  else if (result == 2)
  {
    // ===== デバッグ（後で処理予定） =====
    Serial.print("SPECIAL: ");

    for (int i = 0; i < special.length; i++)
    {
      if (special.raw[i] < 0x10) Serial.print("0");
      Serial.print(special.raw[i], HEX);
      Serial.print(" ");
    }

    Serial.println();
  }
}
