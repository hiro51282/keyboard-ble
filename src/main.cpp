// =============================================
// CH9350 → ESP32 → BLE Bridge
// Keyboard + Mouse 統合版
//
// - CH9350 状態2モード
// - baud = 300000
// - 0x01 -> Keyboard
// - 0x02 -> Mouse
// - 0x80 -> ignore
// - Keyboard : sendReport() 即送信
// - Mouse    : move only / 36ms周期送信
// - click未実装
// - wheel維持
// =============================================

#include <Arduino.h>
#include <BleComboKeyboard.h>
#include <BleComboMouse.h>

HardwareSerial mySerial(2);

BleComboKeyboard bleKeyboard("SimpleBLEDevice", "ESP32", 100);
BleComboMouse bleMouse(&bleKeyboard);

constexpr unsigned long SEND_INTERVAL_US = 36000;

// =============================================
// RawFrame
// =============================================
struct RawFrame
{
    uint8_t type;
    uint8_t data[16];
};

// =============================================
// readRawFrame()
// 状態2専用シンプル parser
//
// keyboard:
// 57 AB 01 [modifier] [reserved] [key1..key6]
//
// mouse:
// 57 AB 02 [button] [dx] [dy] [wheel]
//
// keepalive:
// 57 AB 80 FF
// =============================================
bool readRawFrame(RawFrame &f)
{
    static uint8_t buf[16];
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

        // header待ち
        if (idx < 3)
            continue;

        if (buf[0] != 0x57 || buf[1] != 0xAB)
            continue;

        uint8_t type = buf[2];

        // keyboard : fixed 11 bytes
        if (type == 0x01)
        {
            if (idx < 11)
                continue;

            f.type = 0x01;
            memcpy(f.data, buf, 11);
            idx = 0;
            return true;
        }

        // mouse : fixed 7 bytes
        if (type == 0x02)
        {
            if (idx < 7)
                continue;

            f.type = 0x02;
            memcpy(f.data, buf, 7);
            idx = 0;
            return true;
        }

        // keepalive / status
        if (type == 0x80)
        {
            if (idx < 4)
                continue;

            idx = 0;
            continue;
        }

        // unknown frame
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

    // CH9350
    // RX = GPIO16
    // TX = GPIO17
    mySerial.begin(300000, SERIAL_8N1, 16, 17);

    bleKeyboard.begin();
    bleMouse.begin();

    Serial.println("BLE Keyboard + Mouse bridge start");
    Serial.print("SEND_INTERVAL_US = ");
    Serial.println(SEND_INTERVAL_US);
}

// =============================================
// loop
// =============================================
void loop()
{
    static int accumX = 0;
    static int accumY = 0;
    static int accumWheel = 0;
    static unsigned long lastSendUs = 0;

    RawFrame f;

    // UART frame受信
    if (readRawFrame(f))
    {
        switch (f.type)
        {
        // =============================================
        // Keyboard route
        // =============================================
        case 0x01:
        {
            if (!bleKeyboard.isConnected())
                break;

            KeyReport report = {0};

            // 57 AB 01 [modifier] [reserved] [key1..key6]
            report.modifiers = f.data[3];

            for (int i = 0; i < 6; i++)
            {
                report.keys[i] = f.data[5 + i];
            }

            bleKeyboard.sendReport(&report);
            break;
        }

        // =============================================
        // Mouse route
        // =============================================
        case 0x02:
        {
            uint8_t buttons = f.data[3];
            int8_t dx = (int8_t)f.data[4];
            int8_t dy = (int8_t)f.data[5];
            int8_t wheel = (int8_t)f.data[6];

            // ---------------------------------------------
            // Mouse button handling
            // 想定:
            // bit0 = Left
            // bit1 = Right
            // bit2 = Middle
            // bit3 = Back
            // bit4 = Forward
            // bit5 = Wheel Left（横スクロール左 / 仮説）
            // bit6 = Wheel Right（横スクロール右 / 仮説）
            // ※ Back / Forward / 横スクロールは環境差あり
            // ---------------------------------------------

            static uint8_t prevButtons = 0;

            auto syncButton = [&](uint8_t mask, uint8_t buttonType)
            {
                bool nowPressed = (buttons & mask) != 0;
                bool prevPressed = (prevButtons & mask) != 0;

                if (nowPressed && !prevPressed)
                {
                    bleMouse.press(buttonType);
                }
                else if (!nowPressed && prevPressed)
                {
                    bleMouse.release(buttonType);
                }
            };

            syncButton(0x01, MOUSE_LEFT);
            syncButton(0x02, MOUSE_RIGHT);
            syncButton(0x04, MOUSE_MIDDLE);

            // Back / Forward
            // BleComboMouse 側で対応していればそのまま使う
            // 未対応なら要置き換え
            syncButton(0x08, MOUSE_BACK);
            syncButton(0x10, MOUSE_FORWARD);

            prevButtons = buttons;

            // move / wheel は蓄積して周期送信
            accumX += dx;
            accumY += dy;
            accumWheel += wheel;
            break;
        }
        }
    }

    // =============================================
    // Mouse periodic send
    // =============================================
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
