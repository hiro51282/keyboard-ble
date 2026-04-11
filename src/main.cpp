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
// ===== 拡張戻り値 =====
enum FrameType
{
  FRAME_NONE = 0,
  FRAME_STATE = 1,
  FRAME_SPECIAL = 2,
  FRAME_MAPPED = 3
};

struct FrameResult
{
  FrameType type;
  KeyState state;
  SpecialFrame special;
  uint8_t mappedKey;
};

// ===== Fキー変換テーブル =====
struct FKeyMap {
  uint8_t mode;   // buf[4]
  uint8_t len;    // buf[3]
  uint8_t k1;
  uint8_t k2;
  uint8_t mod;    // 期待modifier（STATE用）
  uint8_t hid;
};


FKeyMap fkeyMap[] = {
  // --- SPECIAL系（そのまま来るやつ）---
  {0x31, 0x08, 0x24, 0x02, 0x00, KEY_F1},
  {0x31, 0x08, 0x23, 0x02, 0x00, KEY_F2},
  {0x31, 0x08, 0x21, 0x02, 0x00, KEY_F5},
  {0x31, 0x08, 0x83, 0x01, 0x00, KEY_F9},
  {0x31, 0x08, 0xB6, 0x00, 0x00, KEY_F10},
  {0x31, 0x08, 0xCD, 0x00, 0x00, KEY_F11},
  {0x31, 0x08, 0xB5, 0x00, 0x00, KEY_F12},

  // --- STATE混入系（擬似Fキー）---
  // それぞれ期待modifierを追加
  {0x11, 0x0B, 0x00, 0x2B, 0x04, KEY_F3}, // Alt+Tab
  {0x11, 0x0B, 0x00, 0x65, 0x00, KEY_F4},
  {0x11, 0x0B, 0x00, 0x07, 0x08, KEY_F6}, // Win系
  {0x11, 0x0B, 0x00, 0x52, 0x08, KEY_F7},
  {0x11, 0x0B, 0x00, 0x13, 0x08, KEY_F8},
};

bool matchFKeyState(const KeyState &s, uint8_t &outKey)
{
  static uint8_t prevKey = 0x00;
  static uint8_t prevMod = 0x00;

  uint8_t k = s.keys[0];

  for (auto &m : fkeyMap)
  {
    if (m.mode == 0x11 &&
        m.k2 == k &&
        m.mod == s.modifiers)
    {
      // エッジ検出（押された瞬間のみ）
      if (k != 0x00 && (prevKey != k || prevMod != s.modifiers))
      {
        outKey = m.hid;
        prevKey = k;
        prevMod = s.modifiers;
        return true;
      }
    }
  }

  // 状態更新
  prevKey = k;
  prevMod = s.modifiers;

  return false;
}


bool matchFKeySpecial(const SpecialFrame &f, uint8_t &outKey)
{
  if (f.length < 8) return false;

  uint8_t mode = f.raw[4];
  uint8_t len  = f.raw[3];
  uint8_t k1   = f.raw[6];
  uint8_t k2   = f.raw[7];

  for (auto &m : fkeyMap)
  {
    if (m.mode == mode && m.len == len && m.k1 == k1 && m.k2 == k2)
    {
      outKey = m.hid;
      return true;
    }
  }
  return false;
}

FrameType readFrame(FrameResult &out)
{
  static uint8_t buf[64];
  static int idx = 0;

  while (mySerial.available())
  {
    uint8_t d = mySerial.read();

    if (d == 0x57)
    {
      idx = 0;
    }

    if (idx < (int)sizeof(buf))
    {
      buf[idx++] = d;
    }

    if (idx < 4)
      continue;

    if (buf[0] != 0x57 || buf[1] != 0xAB)
      continue;

    uint8_t type = buf[2];
    uint8_t len  = buf[3];

    if (idx < (len + 4))
      continue;

    if (type == 0x82)
    {
      idx = 0;
      return FRAME_NONE;
    }

    if (type == 0x88)
    {
      // ===== STATE =====
      if (len >= 0x0B)
      {
        out.state.modifiers = buf[5];
        for (int i = 0; i < 6; i++)
        {
          out.state.keys[i] = buf[7 + i];
        }

        uint8_t key;
        if (matchFKeyState(out.state, key))
        {
          out.type = FRAME_MAPPED;
          out.mappedKey = key;
          idx = 0;
          return FRAME_MAPPED;
        }

        out.type = FRAME_STATE;
        idx = 0;
        return FRAME_STATE;
      }
      // ===== SPECIAL =====
      else
      {
        out.special.length = (len + 4 < sizeof(out.special.raw)) ? (len + 4) : sizeof(out.special.raw);

        for (int i = 0; i < out.special.length; i++)
        {
          out.special.raw[i] = buf[i];
        }

        uint8_t key;
        if (matchFKeySpecial(out.special, key))
        {
          out.type = FRAME_MAPPED;
          out.mappedKey = key;
          idx = 0;
          return FRAME_MAPPED;
        }

        out.type = FRAME_SPECIAL;
        idx = 0;
        return FRAME_SPECIAL;
      }
    }

    idx = 0;
  }

  return FRAME_NONE;
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
  FrameResult r;
  FrameType result = readFrame(r);

  switch (result)
  {
    case FRAME_MAPPED:
      if (bleKeyboard.isConnected())
      {
        bleKeyboard.press(r.mappedKey);
        bleKeyboard.release(r.mappedKey);
      }
      break;

    case FRAME_STATE:
    {
      // ===== デバッグ表示 =====
      Serial.print("STATE: mod=");
      Serial.print(r.state.modifiers, HEX);
      Serial.print(" keys=");

      for (int i = 0; i < 6; i++)
      {
        if (r.state.keys[i] < 0x10) Serial.print("0");
        Serial.print(r.state.keys[i], HEX);
        Serial.print(" ");
      }
      Serial.println();

      // ===== 先行modifier抑制（Fキー候補） =====
      bool suppressModifier = false;
      if (r.state.keys[0] == 0x00 && r.state.modifiers != 0x00)
      {
        for (auto &m : fkeyMap)
        {
          if (m.mode == 0x11 && m.mod == r.state.modifiers)
          {
            suppressModifier = true;
            break;
          }
        }
      }

      if (suppressModifier)
      {
        break;
      }

      if (bleKeyboard.isConnected())
      {
        KeyReport report = {0};
        report.modifiers = r.state.modifiers;

        for (int i = 0; i < 6; i++)
        {
          report.keys[i] = r.state.keys[i];
        }

        bleKeyboard.sendReport(&report);
      }
      break;
    }
    case FRAME_SPECIAL:
      Serial.print("SPECIAL: ");
      for (int i = 0; i < r.special.length; i++)
      {
        if (r.special.raw[i] < 0x10) Serial.print("0");
        Serial.print(r.special.raw[i], HEX);
        Serial.print(" ");
      }
      Serial.println();
      break;

    case FRAME_NONE:
    default:
      break;
  }
}
