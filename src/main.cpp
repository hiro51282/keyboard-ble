#include <Arduino.h>
#include <NimBLEDevice.h>
#include <BleComboKeyboard.h>
#include <BleComboMouse.h>

HardwareSerial mySerial(2);

BleComboKeyboard bleKeyboard("SimpleBLEDevice", "ESP32", 100);
BleComboMouse bleMouse(&bleKeyboard);

constexpr unsigned long SEND_INTERVAL_US = 7500;
constexpr int SWITCH_PIN = 23;
constexpr uint8_t KEY_SCROLL_LOCK = 0x47;

// =============================================
// enterPairingMode  ボタン長押し（3秒）で呼ぶ
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

    pinMode(SWITCH_PIN, INPUT_PULLUP);

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

    // 物理ボタン（GPIO23）短押し/長押し検出
    // 短押し（50ms〜3秒）: switchTarget()
    // 長押し（3秒以上）  : enterPairingMode()
    {
        static bool prevBtn = HIGH;
        static unsigned long btnPressMs = 0;
        static bool longPressTriggered = false;

        bool nowBtn = digitalRead(SWITCH_PIN);
        unsigned long nowMs = millis();

        if (prevBtn == HIGH && nowBtn == LOW) {
            btnPressMs = nowMs;
            longPressTriggered = false;
        } else if (nowBtn == LOW && !longPressTriggered && nowMs - btnPressMs >= 3000) {
            enterPairingMode();
            longPressTriggered = true;
        } else if (prevBtn == LOW && nowBtn == HIGH) {
            if (!longPressTriggered && nowMs - btnPressMs >= 50) {
                switchTarget();
            }
        }

        prevBtn = nowBtn;
    }

    static int accumX = 0;
    static int accumY = 0;
    static int accumWheel = 0;
    static unsigned long lastSendUs = 0;

    // ScrollLock 短押し/長押し状態（フレーム処理の外で長押し時間チェックするためここで宣言）
    static bool scrollLockDown = false;
    static unsigned long scrollLockPressMs = 0;
    static bool scrollLockLongTriggered = false;

    // ScrollLock 長押しチェック（CH9350がリピートを送らなくてもloop内で確認）
    if (scrollLockDown && !scrollLockLongTriggered && millis() - scrollLockPressMs >= 3000)
    {
        enterPairingMode();
        scrollLockLongTriggered = true;
    }

    RawFrame f;

    if (readRawFrame(f))
    {
        switch (f.type)
        {
        case 0x01:
        {
            // ScrollLock 短押し（切り替え）/ 長押し（ペアリングモード）
            {
                bool hasScrollLock = false;
                for (int i = 0; i < 6; i++)
                {
                    if (f.data[5 + i] == KEY_SCROLL_LOCK) { hasScrollLock = true; break; }
                }
                if (hasScrollLock && !scrollLockDown) {
                    // キーダウン
                    scrollLockDown = true;
                    scrollLockPressMs = millis();
                    scrollLockLongTriggered = false;
                } else if (!hasScrollLock && scrollLockDown) {
                    // キーアップ
                    scrollLockDown = false;
                    if (!scrollLockLongTriggered && millis() - scrollLockPressMs >= 50)
                        switchTarget();
                }
            }

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