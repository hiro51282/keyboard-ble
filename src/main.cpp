#include <Arduino.h>
#include <NimBLEDevice.h>
#include <BleComboKeyboard.h>
#include <BleComboMouse.h>
#include "config.h"  // CONFIG_USER_ID（.gitignore済み・雛形は config.h.example）

HardwareSerial mySerial(2);

BleComboKeyboard bleKeyboard("SimpleBLEDevice", "ESP32", 100);
BleComboMouse bleMouse(&bleKeyboard);

constexpr unsigned long SEND_INTERVAL_US = 7500;
constexpr int BUTTON_PIN = 21;  // タクトスイッチ（INPUT_PULLUP / 押下=LOW）

// =============================================
// enterPairingMode  ボタン長押し（2秒）で呼ぶ
//   全bond削除 → 切断 → 通常アドバタイズ
//   2台を順番にペアリングして bond 2件を揃える
// =============================================
void enterPairingMode()
{
    NimBLEServer* pServer = bleKeyboard.getServer();
    if (!pServer) {
        Serial.println("Pairing: server not ready");
        return;
    }

    Serial.println("Pairing: entering pairing mode");

    NimBLEAdvertising* pAdv = pServer->getAdvertising();
    pAdv->stop();
    pAdv->setScanFilter(false, false);  // ホワイトリストフィルタをリセット

    pServer->advertiseOnDisconnect(false);

    uint16_t connHandle = bleKeyboard.getConnHandle();
    if (bleKeyboard.isConnected() && connHandle != 0xFFFF) {
        pServer->disconnect(connHandle);
        delay(500);  // 切断完了を待つ
    }

    NimBLEDevice::deleteAllBonds();
    Serial.println("Pairing: all bonds deleted");

    pAdv->start();
    pServer->advertiseOnDisconnect(true);

    Serial.println("Pairing: advertising started, pair your devices now");
}

// =============================================
// switchTarget
// =============================================
void switchTarget()
{
    NimBLEServer* pServer = bleKeyboard.getServer();
    if (!pServer) {
        Serial.println("Switch: server not ready");
        return;
    }

    int numBonds = NimBLEDevice::getNumBonds();
    Serial.printf("Switch: %d bonded device(s)\n", numBonds);
    if (numBonds < 2) {
        Serial.println("Switch: need 2+ bonded devices");
        return;
    }

    std::string currentAddrStr = bleKeyboard.getPeerAddress().toString();
    NimBLEAddress nextAddr;
    bool found = false;

    for (int i = 0; i < numBonds; i++) {
        NimBLEAddress addr = NimBLEDevice::getBondedAddress(i);
        if (addr.toString() != currentAddrStr) {
            nextAddr = addr;
            found = true;
            break;
        }
    }

    if (!found) {
        Serial.println("Switch: no other bonded device");
        return;
    }

    Serial.printf("Switch: -> %s\n", nextAddr.toString().c_str());

    NimBLEAdvertising* pAdv = pServer->getAdvertising();

    // ble_gap_wl_set はアドバタイズ中に呼べないため先に停止
    pAdv->stop();

    // 次のPCのみ接続を許可するホワイトリストを設定
    NimBLEDevice::whiteListAdd(nextAddr);
    pAdv->setScanFilter(false, true);  // 接続要求のみホワイトリストフィルタ

    // 切断後の自動再起動を一時無効化（切断 → 自動アドバタイズを防ぐ）
    pServer->advertiseOnDisconnect(false);

    uint16_t connHandle = bleKeyboard.getConnHandle();
    if (bleKeyboard.isConnected() && connHandle != 0xFFFF) {
        pServer->disconnect(connHandle);
    }

    // ホワイトリストフィルタ付きでアドバタイズ再開
    // → 前のPCからの接続要求は拒否、次のPCのみ接続可
    pAdv->start();

    // onConnect でホワイトリストをリセットするので、以降は自動再起動も有効に戻す
    pServer->advertiseOnDisconnect(true);
}

// =============================================
// asciiToJisHid
//   ASCII文字 → JIS配列ホストで意図通りに入力されるHID usage を返す。
//   shift に Shift要否を返す。usage==0 は非対応文字。
//   ※ライブラリの write() はUS配列前提なので記号がズレる。JISホスト向けに
//     物理キーのusageを直接指定して送る（英数字はUS/JIS共通）。
// =============================================
static uint8_t asciiToJisHid(char c, bool &shift)
{
    shift = false;
    if (c >= 'a' && c <= 'z') { return 0x04 + (c - 'a'); }
    if (c >= 'A' && c <= 'Z') { shift = true; return 0x04 + (c - 'A'); }
    if (c >= '1' && c <= '9') { return 0x1E + (c - '1'); }
    if (c == '0') { return 0x27; }

    switch (c)
    {
        case ' ':  return 0x2C;
        case '\n': return 0x28;  // Enter
        case '\t': return 0x2B;  // Tab
        // 数字段 Shift（JIS）
        case '!':  shift = true; return 0x1E;
        case '"':  shift = true; return 0x1F;
        case '#':  shift = true; return 0x20;
        case '$':  shift = true; return 0x21;
        case '%':  shift = true; return 0x22;
        case '&':  shift = true; return 0x23;
        case '\'': shift = true; return 0x24;
        case '(':  shift = true; return 0x25;
        case ')':  shift = true; return 0x26;
        // JIS固有の記号配置
        case '-':  return 0x2D;
        case '=':  shift = true; return 0x2D;
        case '^':  return 0x2E;
        case '~':  shift = true; return 0x2E;
        case '@':  return 0x2F;              // ★ JISは Shiftなし単独キー
        case '`':  shift = true; return 0x2F;
        case '[':  return 0x30;
        case '{':  shift = true; return 0x30;
        case ']':  return 0x31;
        case '}':  shift = true; return 0x31;
        case ';':  return 0x33;
        case '+':  shift = true; return 0x33;
        case ':':  return 0x34;
        case '*':  shift = true; return 0x34;
        case ',':  return 0x36;
        case '<':  shift = true; return 0x36;
        case '.':  return 0x37;
        case '>':  shift = true; return 0x37;
        case '/':  return 0x38;
        case '?':  shift = true; return 0x38;
        case '\\': return 0x87;              // ろ キー（International1）
        case '_':  shift = true; return 0x87;
        case '|':  shift = true; return 0x89;  // ¥ キー（International3）
    }
    return 0;  // 非対応
}

// =============================================
// typeUserId  ボタン単押しで CONFIG_USER_ID をHID送信（JIS配列ホスト向け）
//   1文字ずつ keydown→keyup。BLE notify取りこぼし防止に軽くディレイを挟む。
// =============================================
void typeUserId()
{
    if (!bleKeyboard.isConnected()) {
        Serial.println("Type: not connected");
        return;
    }

    Serial.printf("Type: sending user id (%u chars)\n",
                  (unsigned)(sizeof(CONFIG_USER_ID) - 1));

    bleKeyboard.releaseAll();
    for (const char* p = CONFIG_USER_ID; *p != '\0'; ++p) {
        bool shift = false;
        uint8_t usage = asciiToJisHid(*p, shift);
        if (usage == 0) {
            Serial.printf("Type: skip unsupported char 0x%02X\n", (uint8_t)*p);
            continue;
        }

        KeyReport rep = {0};
        rep.modifiers = shift ? 0x02 : 0x00;  // 0x02 = 左Shift
        rep.keys[0] = usage;
        bleKeyboard.sendReport(&rep);
        delay(8);

        KeyReport up = {0};
        bleKeyboard.sendReport(&up);
        delay(8);
    }
}

// =============================================
// RawFrame
// =============================================
struct RawFrame
{
    uint8_t type;
    uint8_t data[16];
};

bool readRawFrame(RawFrame &f)
{
    static uint8_t buf[16];
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

        if (idx < 3)
            continue;

        if (buf[0] != 0x57 || buf[1] != 0xAB)
            continue;

        uint8_t type = buf[2];

        if (type == 0x01)
        {
            if (idx < 11)
                continue;

            f.type = 0x01;
            memcpy(f.data, buf, 11);
            idx = 0;
            return true;
        }

        if (type == 0x02)
        {
            if (idx < 7)
                continue;

            f.type = 0x02;
            memcpy(f.data, buf, 7);
            idx = 0;
            return true;
        }

        if (type == 0x80)
        {
            if (idx < 4)
                continue;
            idx = 0;
            continue;
        }

        idx = 0;
    }

    return false;
}

// =============================================
// setup
// =============================================
void setup()
{
    Serial.begin(115200);
    mySerial.begin(300000, SERIAL_8N1, 16, 17);

    pinMode(BUTTON_PIN, INPUT_PULLUP);

    bleKeyboard.begin();
    bleMouse.begin();

    Serial.println("=== BLE Bridge Start ===");
}

// =============================================
// loop
// =============================================
void loop()
{
    static bool prevConnected = false;
    bool nowConnected = bleKeyboard.isConnected();

    if (nowConnected != prevConnected)
    {
        Serial.print("Connection state -> ");
        Serial.println(nowConnected ? "CONNECTED" : "DISCONNECTED");
        prevConnected = nowConnected;
    }

    // 物理ボタン（GPIO21）単押し / ダブルクリック / 長押し検出
    //   単押し        : typeUserId()        ユーザーID送信
    //   ダブルクリック : switchTarget()      接続先切替
    //   長押し2秒      : enterPairingMode()  ペアリングモード
    // ※単押しはダブル判定のため DOUBLE_GAP_MS 経過してから確定する
    {
        constexpr unsigned long DEBOUNCE_MS   = 20;    // チャタリング除去
        constexpr unsigned long LONG_MS       = 2000;  // 長押し判定
        constexpr unsigned long DOUBLE_GAP_MS = 300;   // ダブルクリック許容間隔

        static bool stable = false;            // デバウンス後の押下状態
        static bool lastReading = false;
        static unsigned long lastChangeMs = 0;

        static bool prevPressed = false;
        static unsigned long pressStartMs = 0;
        static bool longFired = false;
        static bool awaitingSecond = false;    // 1クリック確定、ダブル待ち
        static unsigned long firstReleaseMs = 0;

        unsigned long nowMs = millis();
        bool reading = (digitalRead(BUTTON_PIN) == LOW);  // 押下=LOW

        // デバウンス
        if (reading != lastReading) {
            lastReading = reading;
            lastChangeMs = nowMs;
        }
        if (nowMs - lastChangeMs >= DEBOUNCE_MS) {
            stable = reading;
        }

        // 押下開始
        if (stable && !prevPressed) {
            pressStartMs = nowMs;
            longFired = false;
        }

        // 長押し（押しっぱなしで2秒）
        if (stable && !longFired && nowMs - pressStartMs >= LONG_MS) {
            enterPairingMode();
            longFired = true;
            awaitingSecond = false;  // 保留中のクリックは破棄
        }

        // 離した
        if (!stable && prevPressed && !longFired) {
            if (awaitingSecond && nowMs - firstReleaseMs <= DOUBLE_GAP_MS) {
                switchTarget();          // 2回目 = ダブルクリック
                awaitingSecond = false;
            } else {
                awaitingSecond = true;   // 1回目 = ダブル待ちへ
                firstReleaseMs = nowMs;
            }
        }

        // 単押し確定（ダブルが来ないままタイムアウト）
        if (awaitingSecond && !stable && nowMs - firstReleaseMs > DOUBLE_GAP_MS) {
            typeUserId();
            awaitingSecond = false;
        }

        prevPressed = stable;
    }

    static int accumX = 0;
    static int accumY = 0;
    static int accumWheel = 0;
    static unsigned long lastSendUs = 0;

    RawFrame f;

    if (readRawFrame(f))
    {
        switch (f.type)
        {
        case 0x01:
        {
            if (!bleKeyboard.isConnected())
                break;

            KeyReport report = {0};
            report.modifiers = f.data[3];

            for (int i = 0; i < 6; i++)
                report.keys[i] = f.data[5 + i];

            bleKeyboard.sendReport(&report);
            break;
        }

        case 0x02:
        {
            uint8_t buttons = f.data[3];
            int8_t dx = (int8_t)f.data[4];
            int8_t dy = (int8_t)f.data[5];
            int8_t wheel = (int8_t)f.data[6];

            static uint8_t prevButtons = 0;

            auto syncButton = [&](uint8_t mask, uint8_t buttonType)
            {
                bool nowPressed = (buttons & mask) != 0;
                bool prevPressed = (prevButtons & mask) != 0;

                if (nowPressed && !prevPressed)
                    bleMouse.press(buttonType);
                else if (!nowPressed && prevPressed)
                    bleMouse.release(buttonType);
            };

            syncButton(0x01, MOUSE_LEFT);
            syncButton(0x02, MOUSE_RIGHT);
            syncButton(0x04, MOUSE_MIDDLE);
            syncButton(0x08, MOUSE_BACK);
            syncButton(0x10, MOUSE_FORWARD);

            prevButtons = buttons;

            accumX += dx;
            accumY += dy;
            accumWheel += wheel;
            break;
        }
        }
    }

    if (!bleKeyboard.isConnected())
        return;

    unsigned long now = micros();

    if (now - lastSendUs < SEND_INTERVAL_US)
        return;

    lastSendUs = now;

    if (accumX == 0 && accumY == 0 && accumWheel == 0)
        return;

    int sendX = constrain(accumX, -127, 127);
    int sendY = constrain(accumY, -127, 127);
    int sendWheel = constrain(accumWheel, -127, 127);

    bleMouse.move(sendX, sendY, sendWheel);

    accumX -= sendX;
    accumY -= sendY;
    accumWheel -= sendWheel;
}