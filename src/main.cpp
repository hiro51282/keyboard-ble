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
// Debug Hardware UI
// =============================================
constexpr int BUTTON_PIN = 4;
constexpr int LED_PIN = 2;

// =============================================
// Device state / Button event
// =============================================
enum class DeviceState
{
    CONNECTED,
    ADVERTISING,
    PAIR_MODE,
    ERROR
};

enum class ButtonEvent
{
    NONE,
    SHORT_PRESS,
    LONG_PRESS,
    VERY_LONG_PRESS
};

// =============================================
// Pair mode helper
// Phase 3:
// いまは state 遷移 + ログのみ
// 次段階で BleCombo 側に restartAdvertising()
// を生やしてここから呼ぶ
// =============================================
void requestPairMode()
{
    Serial.println("requestPairMode(): advertising restart pending");
    // TODO:
    // bleKeyboard.restartAdvertising();
}

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

    // Debug button / LED
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    Serial.println("BLE Keyboard + Mouse bridge start");
    Serial.print("SEND_INTERVAL_US = ");
    Serial.println(SEND_INTERVAL_US);
}

// =============================================
// loop
// =============================================
void loop()
{
    // =============================================
    // State machine (Phase 1)
    // まずは enum を導入して状態を明示化する
    // 実際の button event / pair mode 制御は次段階
    // =============================================
    static DeviceState currentState = DeviceState::ADVERTISING;

    // 初回および通常時は実接続状態を反映
    // PAIR_MODE 中だけ明示的に状態保持する
    if (currentState != DeviceState::PAIR_MODE)
    {
        currentState = bleKeyboard.isConnected()
                           ? DeviceState::CONNECTED
                           : DeviceState::ADVERTISING;
    }

    // =============================================
    // Button event detection (Phase 2)
    // NONE / SHORT / LONG / VERY_LONG を判定
    // まだ動作は紐づけず、まずは観測だけ行う
    // =============================================
    static bool prevPressed = false;
    static unsigned long pressStartMs = 0;

    ButtonEvent buttonEvent = ButtonEvent::NONE;

    bool buttonPressed = (digitalRead(BUTTON_PIN) == LOW);

    if (buttonPressed && !prevPressed)
    {
        pressStartMs = millis();
    }

    if (!buttonPressed && prevPressed)
    {
        unsigned long pressTime = millis() - pressStartMs;

        if (pressTime > 5000)
        {
            buttonEvent = ButtonEvent::VERY_LONG_PRESS;
        }
        else if (pressTime > 1500)
        {
            buttonEvent = ButtonEvent::LONG_PRESS;
        }
        else
        {
            buttonEvent = ButtonEvent::SHORT_PRESS;
        }
    }

    prevPressed = buttonPressed;

    switch (buttonEvent)
    {
    case ButtonEvent::SHORT_PRESS:
        Serial.println("ButtonEvent: SHORT_PRESS");
        // 将来的に Host切替へ
        break;

    case ButtonEvent::LONG_PRESS:
        Serial.println("ButtonEvent: LONG_PRESS -> PAIR_MODE");
        currentState = DeviceState::PAIR_MODE;
        requestPairMode();
        break;

    case ButtonEvent::VERY_LONG_PRESS:
        Serial.println("ButtonEvent: VERY_LONG_PRESS");
        // 将来的に clearBond() / factory reset
        break;

    default:
        break;
    }

    // =============================================
    // LED state management
    // connected     : LED OFF
    // advertising   : LED blink
    // disconnected  : LED ON（初期状態 / 未接続）
    // ※ pair mode は後で追加
    // =============================================
    static unsigned long lastBlinkMs = 0;
    static bool ledState = false;

    bool connected = bleKeyboard.isConnected();

switch (currentState)
{
case DeviceState::CONNECTED:
    // 接続済 → 消灯
    ledState = false;
    digitalWrite(LED_PIN, LOW);
    break;

case DeviceState::ADVERTISING:
{
    // advertising → ゆっくり点滅
    unsigned long nowMs = millis();

    if (nowMs - lastBlinkMs > 500)
    {
        lastBlinkMs = nowMs;
        ledState = !ledState;
        digitalWrite(LED_PIN, ledState ? HIGH : LOW);
    }
    break;
}

case DeviceState::PAIR_MODE:
{
    // pair mode → 高速点滅
    unsigned long nowMs = millis();

    if (nowMs - lastBlinkMs > 120)
    {
        lastBlinkMs = nowMs;
        ledState = !ledState;
        digitalWrite(LED_PIN, ledState ? HIGH : LOW);
    }
    break;
}

case DeviceState::ERROR:
default:
    // 仮: ERROR は点灯固定
    ledState = true;
    digitalWrite(LED_PIN, HIGH);
    break;
}
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
