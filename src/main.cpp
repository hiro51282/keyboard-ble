#include <Arduino.h>

HardwareSerial mySerial(2);

uint8_t buf[8];
int idx = 0;

// ===== Modifier表示 =====
void printModifiers(uint8_t m2)
{
  if (m2 & 0x01)
    Serial.print("Ctrl(L) ");
  if (m2 & 0x10)
    Serial.print("Ctrl(R) ");
  if (m2 & 0x02)
    Serial.print("Shift(L) ");
  if (m2 & 0x20)
    Serial.print("Shift(R) ");
  if (m2 & 0x04)
    Serial.print("Alt(L) ");
  if (m2 & 0x40)
    Serial.print("Alt(R) ");
  if (m2 & 0x08)
    Serial.print("Win ");
}

// ===== Fキー判定 =====
bool isFunctionKey(uint8_t m1, uint8_t m2, uint8_t k1, uint8_t k2)
{
  // F1〜F12の特徴パターン
  if (
      (m1 == 0x31 && m2 == 0x03) || // F1,F2,F5,F9〜F12
      (m1 == 0x11)                  // F3,F4,F6〜F8
  )
  {
    return true;
  }
  return false;
}

// ===== マッピング =====
struct KeyEvent
{
  uint8_t m1;
  uint8_t m2;
  uint8_t k1;
  uint8_t k2;
  const char *name;
};

KeyEvent keyMap[] = {
    {0x31, 0x03, 0x24, 0x02, "F1"},
    {0x31, 0x03, 0x23, 0x02, "F2"},
    {0x11, 0x04, 0x00, 0x2B, "F3"},
    {0x11, 0x00, 0x00, 0x65, "F4"},
    {0x31, 0x03, 0x21, 0x02, "F5"},

    {0x11, 0x08, 0x00, 0x07, "F6"},
    {0x11, 0x08, 0x00, 0x52, "F7"},
    {0x11, 0x08, 0x00, 0x13, "F8"},

    {0x31, 0x03, 0x83, 0x01, "F9"},
    {0x31, 0x03, 0xB6, 0x00, "F10"},
    {0x31, 0x03, 0xCD, 0x00, "F11"},
    {0x31, 0x03, 0xB5, 0x00, "F12"},
};

const int keyMapSize = sizeof(keyMap) / sizeof(KeyEvent);

// ===== 検索 =====
const char *findKey(uint8_t m1, uint8_t m2, uint8_t k1, uint8_t k2)
{
  for (int i = 0; i < keyMapSize; i++)
  {
    if (keyMap[i].m1 == m1 &&
        keyMap[i].m2 == m2 &&
        keyMap[i].k1 == k1 &&
        keyMap[i].k2 == k2)
    {
      return keyMap[i].name;
    }
  }
  return nullptr;
}

void setup()
{
  Serial.begin(115200);
  mySerial.begin(115200, SERIAL_8N1, 16, 17);
}

void loop()
{
  static uint8_t prev_m2 = 0;

  while (mySerial.available())
  {
    uint8_t d = mySerial.read();

    if (d == 0x57)
    {
      idx = 0;
    }

    if (idx < 8)
    {
      buf[idx++] = d;
    }

    if (idx == 8)
    {
      uint8_t m1 = buf[4];
      uint8_t m2 = buf[5];
      uint8_t k1 = buf[6];
      uint8_t k2 = buf[7];

      // ===== Modifier変化のみ表示 =====
      if (m2 != prev_m2)
      {
        // 押された瞬間だけ
        uint8_t pressed = m2 & (~prev_m2);

        if (pressed)
        {
          Serial.print("Modifier DOWN: ");
          printModifiers(pressed);
          Serial.println();
        }

        prev_m2 = m2;
      }
      // ===== KEYUP無視 =====
      if (k1 == 0x00 && k2 == 0x00)
      {
        idx = 0;
        return;
      }

      const char *key = findKey(m1, m2, k1, k2);

      if (key)
      {
        // ===== Modifier正規化 =====
        if (isFunctionKey(m1, m2, k1, k2))
        {
          Serial.print(key);
          Serial.println(" 受信！（modifier除去）");
        }
        else
        {
          Serial.print(key);
          Serial.print(" 受信！（");
          printModifiers(m2);
          Serial.println("）");
        }
      }
      else
      {
        Serial.print("UNKNOWN: ");
        Serial.print("m1=");
        Serial.print(m1, HEX);
        Serial.print(" m2=");
        Serial.print(m2, HEX);
        Serial.print(" k1=");
        Serial.print(k1, HEX);
        Serial.print(" k2=");
        Serial.println(k2, HEX);
      }

      idx = 0;
    }
  }
}