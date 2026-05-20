# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is an ESP32-based BLE HID bridge that converts USB keyboard and mouse input (via CH9350 UART adapter) into Bluetooth LE HID reports. The goal is reliable daily-use wireless input, not gaming performance.

**Data flow**: USB Keyboard/Mouse → CH9350 (UART, 300000 baud) → ESP32 → BLE HID → PC

## Build & Development

### Prerequisites
- PlatformIO (`pip install platformio`)
- ESP32 board support
- USB cable for flashing

### Common Commands

**Build**:
```bash
pio run -e esp32dev
```

**Build and upload to device**:
```bash
pio run -e esp32dev -t upload
```

**Monitor serial output** (after flashing, shows debug logs and connection status):
```bash
pio device monitor -b 115200
```

**Clean build artifacts**:
```bash
pio run -e esp32dev -t clean
```

### Docker Setup
The project includes Docker scripts for isolated builds:
- `01_make_container.sh` — Build Docker image
- `02_enter_container.sh` — Enter container environment  
- `05_build_flash_monitor.sh` — All-in-one build, flash, and monitor

## Architecture

### Main Application (src/main.cpp)

The main loop handles:
1. **UART frame reception** from CH9350 via Serial2 (GPIO16/17 at 300000 baud)
2. **Frame parsing** via `readRawFrame()` which extracts three frame types:
   - `0x01` — Keyboard frame (11 bytes): `57 AB 01 [modifier] [reserved] [key1-6]`
   - `0x02` — Mouse frame (7 bytes): `57 AB 02 [buttons] [dx] [dy] [wheel]`
   - `0x80` — Keepalive/status (ignored, 4 bytes)
3. **HID report generation** using BleCombo library
4. **BLE transmission** when connected

### BLE HID Library (lib/ESP32-NimBLE-Combo)

Vendored, NimBLE-based implementation of combined keyboard+mouse HID device:

- **BleComboKeyboard**: Manages HID keyboard interface, stores connection state
- **BleComboMouse**: Wraps keyboard instance, sends mouse HID reports
- **BleCombo**: Base HID descriptor and characteristic management

**Why NimBLE?** Lower notify queue congestion and better stability than Bluedroid for HID, allowing reliable keyboard input and smooth mouse movement.

**Key modification for Japanese layout**: HID descriptor LOGICAL_MAXIMUM and USAGE_MAXIMUM extended to 0xFF (256 keys) instead of standard 101-key US layout. This prevents key mapping loss on Japanese keyboards.

### Mouse Acceleration Buffer

Mouse movement intentionally accumulates instead of sending every frame:

```cpp
constexpr unsigned long SEND_INTERVAL_US = 25000;  // 25ms intervals
```

**Why?** BLE HID notify queues can overflow with high-frequency reports, causing dropped input and latency. The accumulation buffer lets the device batch motion reports at 25ms intervals, trading microsecond precision for queue stability.

- `accumX`, `accumY`, `accumWheel` accumulate signed deltas from CH9350
- Every 25ms, send constrained report (±127 range) and subtract sent amount from accumulator
- Prevents loss of motion data while respecting BLE bandwidth limits

## Key Implementation Details

### Frame Parsing (readRawFrame)

Uses simple synchronization on `0x57 0xAB` preamble, then reads frame-type-specific byte counts:
- Keyboard: 11 bytes total
- Mouse: 7 bytes total
- Keepalive: 4 bytes (discarded)

Any other type resets state. Parser is intentionally simple (not "smart") — avoids inheriting old legacy code quirks.

### Keyboard Report
- Byte 3: Modifier flags (Shift, Ctrl, Alt, GUI)
- Bytes 5–10: Up to 6 simultaneous key codes (6KRO)
- Uses `BleComboKeyboard::sendReport()` directly instead of `press()`/`release()` for atomic updates

### Mouse Report
Buttons are edge-triggered (press/release detected from frame-to-frame change):
```cpp
syncButton(0x01, MOUSE_LEFT);
syncButton(0x02, MOUSE_RIGHT);
syncButton(0x04, MOUSE_MIDDLE);
syncButton(0x08, MOUSE_BACK);
syncButton(0x10, MOUSE_FORWARD);
```

Movement and wheel are accumulated and sent periodically.

## Known Constraints

- **Not gaming-grade**: BLE latency + 25ms send interval make this unsuitable for competitive gaming
- **Tilt wheel unsupported**: Horizontal scroll not implemented
- **Single PC**: No device switching logic yet
- **No low-level NimBLE**: Currently using Arduino wrapper; direct ESP-IDF integration is future work

## Testing Strategy

1. **Serial monitor output** — Watch for connection state changes (`CONNECTED` / `DISCONNECTED`)
2. **Manual testing** — Type in text editor, move mouse, test modifier keys and simultaneous presses
3. **Key combinations** — Verify Shift/Ctrl/Alt work with letter keys
4. **Mouse buttons** — Test left, right, middle, back, forward clicks
5. **Scrolling** — Verify wheel works without stuttering

## Debugging

**Connection issues**: Check serial output for `CONNECTED`/`DISCONNECTED` messages. Verify BLE is advertising (visible in OS Bluetooth settings).

**Input lag or dropped keys**: Likely frame parser corruption or UART buffer overflow. Check:
- Baud rate match (CH9350 DIP config and `300000` in code)
- UART GPIO pins (RX=GPIO16, TX=GPIO17)
- Frame preamble detection in `readRawFrame()`

**Mouse stuttering**: May indicate notify queue saturation. The 25ms interval is empirically optimal; faster causes errors, slower feels sluggish.

**Japanese key mapping issues**: Confirm HID descriptor in `BleCombo.cpp` has extended LOGICAL_MAXIMUM (0xFF) — this was a critical fix for full keyboard support.
