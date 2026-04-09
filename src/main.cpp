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

// ===== Modifier適用 =====
void applyModifiers(uint8_t m2)
{
  if (m2 & 0x01)
    bleKeyboard.press(KEY_LEFT_CTRL);
  if (m2 & 0x10)
    bleKeyboard.press(KEY_RIGHT_CTRL);
  if (m2 & 0x02)
    bleKeyboard.press(KEY_LEFT_SHIFT);
  if (m2 & 0x20)
    bleKeyboard.press(KEY_RIGHT_SHIFT);
  if (m2 & 0x04)
    bleKeyboard.press(KEY_LEFT_ALT);
  if (m2 & 0x40)
    bleKeyboard.press(KEY_RIGHT_ALT);
  if (m2 & 0x08)
    bleKeyboard.press(KEY_LEFT_GUI);
}

// ===== HID変換 =====
char convertHID(uint8_t k2)
{
  if (k2 >= 0x04 && k2 <= 0x1D)
    return 'a' + (k2 - 0x04);

  if (k2 >= 0x1E && k2 <= 0x26)
    return '1' + (k2 - 0x1E);

  if (k2 == 0x27)
    return '0';

  switch (k2)
  {
    case 0x28: return '\n';
    case 0x29: return 27;
    case 0x2A: return '\b';
    case 0x2B: return '\t';
    case 0x2C: return ' ';
    case 0x4C: return 127;

    case 0x2D: return '-';
    case 0x2E: return '=';
    case 0x2F: return '[';
    case 0x30: return ']';
    case 0x31: return '\\';
    case 0x33: return ';';
    case 0x34: return '\'';
    case 0x36: return ',';
    case 0x37: return '.';
    case 0x38: return '/';

    case 0x35: return '`';

    default: return 0;
  }
}

// ===== Consumerキー =====
void sendConsumer(uint8_t k1)
{
  if (!bleKeyboard.isConnected())
    return;

  switch (k1)
  {
  case 0xE9:
    bleKeyboard.write(KEY_MEDIA_VOLUME_UP);
    break;
  case 0xEA:
    bleKeyboard.write(KEY_MEDIA_VOLUME_DOWN);
    break;
  case 0xE2:
    bleKeyboard.write(KEY_MEDIA_MUTE);
    break;
  }
}

// ===== Fキー =====
struct KeyEvent
{
  uint8_t m1;
  uint8_t m2;
  uint8_t k1;
  uint8_t k2;
  uint8_t hidKey;
};

KeyEvent keyMap[] = {
    {0x31, 0x03, 0x24, 0x02, KEY_F1},
    {0x31, 0x03, 0x23, 0x02, KEY_F2},
    {0x11, 0x04, 0x00, 0x2B, KEY_F3},
    {0x11, 0x00, 0x00, 0x65, KEY_F4},
    {0x31, 0x03, 0x21, 0x02, KEY_F5},
    {0x11, 0x08, 0x00, 0x07, KEY_F6},
    {0x11, 0x08, 0x00, 0x52, KEY_F7},
    {0x11, 0x08, 0x00, 0x13, KEY_F8},
    {0x31, 0x03, 0x83, 0x01, KEY_F9},
    {0x31, 0x03, 0xB6, 0x00, KEY_F10},
    {0x31, 0x03, 0xCD, 0x00, KEY_F11},
    {0x31, 0x03, 0xB5, 0x00, KEY_F12},
};

bool isFunctionKeyFrame(uint8_t m1)
{
  return (m1 == 0x31 || m1 == 0x11);
}

const KeyEvent *findKey(uint8_t m1, uint8_t m2, uint8_t k1, uint8_t k2)
{
  for (auto &k : keyMap)
  {
    if (k.m1 == m1 &&
        k.k1 == k1 &&
        k.k2 == k2 &&
        k.m2 == m2)
    {
      return &k;
    }
  }
  return nullptr;
}

// ===== KEYDOWN =====
void handleKeyDown(const KeyEvent *key, uint8_t m2, uint8_t k1, uint8_t k2)
{
  if (!bleKeyboard.isConnected()) return;

  // ===== DEBUG INPUT =====
  Serial.printf("INPUT k2=0x%02X m2=0x%02X k1=0x%02X", k2, m2, k1);

  if (key)
  {
    Serial.println("PATH: FKEY");
    bleKeyboard.press(key->hidKey);
    return;
  }

  applyModifiers(m2);

  if (k2 == 0x49) { Serial.println("PATH: INSERT"); bleKeyboard.press(KEY_INSERT); return; }
  if (k2 == 0x4C) { Serial.println("PATH: DELETE"); bleKeyboard.press(KEY_DELETE); return; }
  if (k2 == 0x29) { Serial.println("PATH: ESC"); bleKeyboard.press(KEY_ESC); return; }

  if (k2 == 0x4F) { Serial.println("PATH: RIGHT"); bleKeyboard.press(KEY_RIGHT_ARROW); return; }
  if (k2 == 0x50) { Serial.println("PATH: LEFT"); bleKeyboard.press(KEY_LEFT_ARROW); return; }
  if (k2 == 0x51) { Serial.println("PATH: DOWN"); bleKeyboard.press(KEY_DOWN_ARROW); return; }
  if (k2 == 0x52) { Serial.println("PATH: UP"); bleKeyboard.press(KEY_UP_ARROW); return; }

  if (k2 == 0x32) { Serial.println("PATH: DIRECT_]"); bleKeyboard.press(0x5c); return; }

  //英字配列日本語配列問題で一時的にこの値を割り当て
  if (k2 == 0x89) { Serial.println("PATH: BACKSLASH"); bleKeyboard.press('/'); return; }
  if (k2 == 0x87) { Serial.println("PATH: UNDERSCORE"); bleKeyboard.press('-'); return; }

  if (k2 != 0)
  {
    char c = convertHID(k2);
    Serial.printf("PATH: convertHID -> '%c' (0x%02X)", c, k2);
    if (c != 0)
    {
      bleKeyboard.press(c);
    }
    return;
  }

  Serial.println("PATH: CONSUMER");
  sendConsumer(k1);
}

// ===== KEYUP =====
void handleKeyUp()
{
  if (!bleKeyboard.isConnected())
    return;
  bleKeyboard.releaseAll();
}

// ===== setup =====
void setup()
{
  Serial.begin(115200);
  mySerial.begin(115200, SERIAL_8N1, 16, 17);
  bleKeyboard.begin();
}

static uint8_t prev_k1 = 0;
static uint8_t prev_k2 = 0;
static uint8_t prev_m2 = 0;

// ===== loop =====
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

      if (k1 == 0x00 && k2 == 0x00)
      {
        handleKeyUp();
        prev_k1 = 0;
        prev_k2 = 0;
        prev_m2 = 0;
        idx = 0;
        return;
      }

      if (k1 == prev_k1 && k2 == prev_k2 && m2 == prev_m2)
      {
        idx = 0;
        return;
      }

      const KeyEvent *key = nullptr;
      if (isFunctionKeyFrame(m1))
      {
        key = findKey(m1, m2, k1, k2);
      }

      handleKeyDown(key, m2, k1, k2);

      prev_k1 = k1;
      prev_k2 = k2;
      prev_m2 = m2;

      idx = 0;
    }
  }
}
