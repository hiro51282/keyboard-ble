// =============================================
// CH9350 → ESP32 → BLE Bridge
// Keyboard + Mouse 統合版
// =============================================

#include <Arduino.h>
#include <BleComboKeyboard.h>
#include <BleComboMouse.h>
#include "esp_gap_ble_api.h"

HardwareSerial mySerial(2);

BleComboKeyboard bleKeyboard("SimpleBLEDevice", "ESP32", 100);
BleComboMouse bleMouse(&bleKeyboard);

constexpr unsigned long SEND_INTERVAL_US = 36000;

// =============================================
// Buttons (2ボタン構成)
// =============================================
constexpr int BUTTON_A_PIN = 4;
constexpr int BUTTON_B_PIN = 2;

// =============================================
// Host Mode
// =============================================
enum class HostMode
{
    NONE,
    A,
    B
};

HostMode currentMode = HostMode::NONE;

// =============================================
// Peer Address (NEW)
// =============================================
uint8_t lastPeerAddr[6] = {0};
bool hasPeerAddr = false;

// GAP callback（接続相手取得）
static void gapCallback(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    if (event == ESP_GAP_BLE_AUTH_CMPL_EVT)
    {
        memcpy(lastPeerAddr, param->ble_security.auth_cmpl.bd_addr, 6);
        hasPeerAddr = true;

        Serial.print("Peer address: ");
        for (int i = 0; i < 6; i++)
        {
            Serial.printf("%02X", lastPeerAddr[i]);
            if (i != 5) Serial.print(":");
        }
        Serial.println();
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

    bleKeyboard.begin();
    bleMouse.begin();

    pinMode(BUTTON_A_PIN, INPUT_PULLUP);
    pinMode(BUTTON_B_PIN, INPUT_PULLUP);

    Serial.println("=== BLE Bridge Start ===");

    // GAP callback登録（接続相手取得）
    esp_ble_gap_register_callback(gapCallback);
}

// =============================================
// loop
// =============================================
void loop()
{
    // =============================================
    // Button handling
    // =============================================
    static bool prevA = false;
    static bool prevB = false;

    bool pressedA = (digitalRead(BUTTON_A_PIN) == LOW);
    bool pressedB = (digitalRead(BUTTON_B_PIN) == LOW);

    if (pressedA && !prevA)
    {
        currentMode = HostMode::A;
        Serial.println("Mode -> A");
        bleKeyboard.disconnectAndAdvertise();
    }

    if (pressedB && !prevB)
    {
        currentMode = HostMode::B;
        Serial.println("Mode -> B");
        bleKeyboard.disconnectAndAdvertise();
    }

    prevA = pressedA;
    prevB = pressedB;

    // =============================================
    // Connection debug
    // =============================================
    static bool prevConnected = false;
    bool nowConnected = bleKeyboard.isConnected();

    if (nowConnected != prevConnected)
    {
        Serial.print("Connection state -> ");
        Serial.println(nowConnected ? "CONNECTED" : "DISCONNECTED");

        if (nowConnected)
        {
            Serial.print("Connected in mode: ");
            if (currentMode == HostMode::A) Serial.println("A");
            else if (currentMode == HostMode::B) Serial.println("B");
            else Serial.println("NONE");

            // ★ここに移動（prevConnected更新前）
            Serial.println("[DEBUG] Peer connected (address not yet captured)");
        }

        prevConnected = nowConnected;
    }

    // =============================================
    // Peer tracking (NEW)
    // =============================================
    // 実際のアドレスはGAP callbackで取得される

    // =============================================
    // UART
    // =============================================
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
            {
                report.keys[i] = f.data[5 + i];
            }

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
