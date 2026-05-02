#include <Arduino.h>
#include <BleComboKeyboard.h>
#include <BleComboMouse.h>

HardwareSerial mySerial(2);

BleComboKeyboard bleKeyboard("SimpleBLEDevice", "ESP32", 100);
BleComboMouse bleMouse(&bleKeyboard);

constexpr unsigned long SEND_INTERVAL_US = 25000;

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