// =============================================
// 超シンプル版：有線キーボード → BLE ブリッジ
// （K400の闇は消し去った）
// =============================================

#include <Arduino.h>
#include <BleComboKeyboard.h>
#include <BleComboMouse.h>

HardwareSerial mySerial(2);
BleComboKeyboard bleKeyboard("SimpleBLEDevice", "ESP32", 100);
BleComboMouse bleMouse(&bleKeyboard);
// ===== KeyState =====
struct KeyState
{
  uint8_t modifiers;
  uint8_t keys[6];
};

// ===== RawFrame =====
struct RawFrame
{
  uint8_t type;
  uint8_t len;
  uint8_t data[64];
};

// ===== フレーム取得 =====
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

// ===== setup =====
void setup()
{
  
  mySerial.begin(115200, SERIAL_8N1, 16, 17);
  bleKeyboard.begin();
  bleMouse.begin();
}

// ===== loop（そのまま転送） =====
void loop()
{
  RawFrame f;

  if (!readRawFrame(f)) return;

  // STATEのみ扱う
  if (f.type == 0x88 && f.len >= 0x0B)
  {
    KeyState s;

    s.modifiers = f.data[5];

    for (int i = 0; i < 6; i++)
    {
      s.keys[i] = f.data[7 + i];
    }

    

    // ===== BLE送信（そのまま） =====
    if (bleKeyboard.isConnected())
    {
      KeyReport report = {0};
      report.modifiers = s.modifiers;

      for (int i = 0; i < 6; i++)
      {
        report.keys[i] = s.keys[i];
      }

      bleKeyboard.sendReport(&report);
    }
  }
}
