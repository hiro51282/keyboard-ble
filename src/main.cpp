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
  uint8_t k = s.keys[0];

  Serial.print("[MATCH] input mod=");
  Serial.print(s.modifiers, HEX);
  Serial.print(" key=");
  Serial.println(k, HEX);

  for (auto &m : fkeyMap)
  {
    Serial.print("  map check: m.mod=");
    Serial.print(m.mod, HEX);
    Serial.print(" m.k2=");
    Serial.print(m.k2, HEX);
    Serial.print(" m.hid=");
    Serial.println(m.hid, HEX);

    if (m.mode == 0x11 &&
        m.k2 == k &&
        m.mod == s.modifiers)
    {
      Serial.println("  -> CONDITION MATCH");

      outKey = m.hid;
      return true;
    }
  }

  Serial.println("  -> NO MATCH");
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

// ===== RawFrame（低レベルフレーム） =====
struct RawFrame
{
  uint8_t type;
  uint8_t len;
  uint8_t data[64];
};

// ===== 生フレーム取得（構造のみ） =====
bool readRawFrame(RawFrame &f)
{
  static uint8_t buf[64];
  static int idx = 0;

  while (mySerial.available())
  {
    uint8_t d = mySerial.read();

    if (d == 0x57) idx = 0;

    if (idx < (int)sizeof(buf)) buf[idx++] = d;

    if (idx < 4) continue;

    if (buf[0] != 0x57 || buf[1] != 0xAB) continue;

    uint8_t type = buf[2];
    uint8_t len  = buf[3];

    // 82は4バイトで確定扱い
    if (type == 0x82 && idx >= 4)
    {
      f.type = type;
      f.len = len;
      memcpy(f.data, buf, idx);
      idx = 0;
      return true;
    }

    if (idx < (len + 4)) continue;

    f.type = type;
    f.len  = len;
    memcpy(f.data, buf, len + 4);

    idx = 0;
    return true;
  }

  return false;
}

// ===== フレーム解釈（意味） =====
// ===== フレーム解釈（イベント駆動版） =====
FrameType interpretFrame(const RawFrame &f, FrameResult &out)
{
  // ===== 状態 =====
  static bool measuring = false;
  static int idleCount = 0;

  // ===== IDLE（時間） =====
  if (f.type == 0x82)
  {
    if (measuring)
    {
      idleCount++;
      Serial.print("[82] idle=");
      Serial.println(idleCount);
    }
    return FRAME_NONE;
  }

  // ===== DATA =====
  if (f.type == 0x88)
  {
    // ===== STATE =====
    if (f.len >= 0x0B)
    {
      out.state.modifiers = f.data[5];
      for (int i = 0; i < 6; i++)
      {
        out.state.keys[i] = f.data[7 + i];
      }

      uint8_t mod = out.state.modifiers;
      uint8_t key = out.state.keys[0];
      // ★これ追加（超重要）
      if (measuring && mod == 0x00 && key == 0x00)
      {
        Serial.println(">>> CANCEL (all released)");

        measuring = false;
        idleCount = 0;

        out.type = FRAME_STATE;
        return FRAME_STATE;
      }
      Serial.print("[STATE] mod=");
      Serial.print(mod, HEX);
      Serial.print(" key=");
      Serial.print(key, HEX);
      Serial.print(" idle=");
      Serial.print(idleCount);
      Serial.print(" measuring=");
      Serial.println(measuring ? "Y" : "N");

      // ===== 測定開始（トリガー） =====
      bool isTrigger = ((mod == 0x04 || mod == 0x08) && key == 0x00) || (key == 0x65);

      if (!measuring && isTrigger)
      {
        Serial.println(">>> START MEASURE");
        measuring = true;
        idleCount = 0;
        return FRAME_STATE;
      }

      // ===== 判定 =====
      if (measuring && key != 0x00)
      {
        Serial.print(">>> DECIDE idle=");
        Serial.println(idleCount);

        uint8_t mapped;
        bool isMapped = matchFKeyState(out.state, mapped);

        if (isMapped && idleCount < 10)
        {
          Serial.println(">>> RESULT: MAPPED");
          out.type = FRAME_MAPPED;
          out.mappedKey = mapped;
        }
        else
        {
          Serial.println(">>> RESULT: STATE");
          out.type = FRAME_STATE;
        }

        measuring = false;
        idleCount = 0;
        return out.type;
      }

      // ===== 通常STATE =====
      out.type = FRAME_STATE;
      return FRAME_STATE;
    }

    // ===== SPECIAL =====
    out.special.length = (f.len + 4 < sizeof(out.special.raw)) ? (f.len + 4) : sizeof(out.special.raw);
    for (int i = 0; i < out.special.length; i++)
    {
      out.special.raw[i] = f.data[i];
    }

    uint8_t key;
    if (matchFKeySpecial(out.special, key))
    {
      out.type = FRAME_MAPPED;
      out.mappedKey = key;
      return FRAME_MAPPED;
    }

    out.type = FRAME_SPECIAL;
    return FRAME_SPECIAL;
  }

  return FRAME_NONE;
}

// ===== 統合 =====
FrameType readFrame(FrameResult &out)
{
  RawFrame f;
  if (!readRawFrame(f)) return FRAME_NONE;
  return interpretFrame(f, out);
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
  static KeyState pendingState;
  static bool hasPending = false;
  static bool skipNextState = false;
  FrameResult r;
  FrameType result = readFrame(r);

  switch (result)
  {
case FRAME_MAPPED:
  if (bleKeyboard.isConnected())
  {
    if (hasPending)
    {
      KeyReport empty = {0};
      bleKeyboard.sendReport(&empty);
      hasPending = false;
    }

    // ★ これ追加
    skipNextState = true;

    bleKeyboard.press(r.mappedKey);
    bleKeyboard.release(r.mappedKey);
  }
  break;
  
    case FRAME_STATE:
    {
      // ===== デバッグ =====
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
      if (skipNextState)
      {
        skipNextState = false;
        break; // 完全スキップ
      }
      // ★ 前のpendingを送る
      if (hasPending && bleKeyboard.isConnected())
      {
        KeyReport report = {0};
        report.modifiers = pendingState.modifiers;

        for (int i = 0; i < 6; i++)
        {
          report.keys[i] = pendingState.keys[i];
        }

        bleKeyboard.sendReport(&report);
      }

      // ★ 今のSTATEをpendingにする
      pendingState = r.state;
      hasPending = true;

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
