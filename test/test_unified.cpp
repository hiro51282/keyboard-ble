#include <unity.h>
#include <string.h>
#include <cstdint>

// ============ STRUCTURES ============

struct RawFrame {
    uint8_t type;
    uint8_t data[16];
};

struct KeyReport {
    uint8_t modifiers;
    uint8_t reserved;
    uint8_t keys[6];
};

// ============ MOCK SERIAL ============

class MockSerial {
private:
    uint8_t buffer[256];
    int writeIndex = 0;
    int readIndex = 0;
public:
    int available() {
        return writeIndex - readIndex;
    }

    uint8_t read() {
        if (readIndex < writeIndex) {
            return buffer[readIndex++];
        }
        return 0;
    }

    void addData(const uint8_t* data, int len) {
        memcpy(buffer + writeIndex, data, len);
        writeIndex += len;
    }

    void clear() {
        writeIndex = 0;
        readIndex = 0;
    }
};

MockSerial mockSerial;

// ============ FRAME PARSER ============

bool readRawFrame_test(RawFrame &f) {
    static uint8_t buf[16];
    static int idx = 0;

    while (mockSerial.available()) {
        uint8_t d = mockSerial.read();

        if (d == 0x57) {
            idx = 0;
        }

        if (idx < (int)sizeof(buf)) {
            buf[idx++] = d;
        }

        if (idx < 3)
            continue;

        if (buf[0] != 0x57 || buf[1] != 0xAB)
            continue;

        uint8_t type = buf[2];

        if (type == 0x01) {
            if (idx < 11)
                continue;

            f.type = 0x01;
            memcpy(f.data, buf, 11);
            idx = 0;
            return true;
        }

        if (type == 0x02) {
            if (idx < 7)
                continue;

            f.type = 0x02;
            memcpy(f.data, buf, 7);
            idx = 0;
            return true;
        }

        if (type == 0x80) {
            if (idx < 4)
                continue;
            idx = 0;
            continue;
        }

        idx = 0;
    }

    return false;
}

// ============ CONSTRAIN HELPER ============
template<typename T>
T constrain_val(T amt, T low, T high) {
    return (amt < low) ? low : ((amt > high) ? high : amt);
}

// ============ TEST SETUP/TEARDOWN ============

void setUp(void) {
    mockSerial.clear();
}

void tearDown(void) {
}

// ============ FRAME PARSER TESTS ============

void test_01_keyboard_frame_parsing(void) {
    RawFrame f;
    uint8_t keyboardFrame[] = {0x57, 0xAB, 0x01, 0x02, 0x00, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09};

    mockSerial.addData(keyboardFrame, sizeof(keyboardFrame));
    bool result = readRawFrame_test(f);

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_UINT8(0x01, f.type);
    TEST_ASSERT_EQUAL_UINT8(0x02, f.data[3]);
    TEST_ASSERT_EQUAL_UINT8(0x04, f.data[5]);
    TEST_ASSERT_EQUAL_UINT8(0x09, f.data[10]);
}

void test_02_mouse_frame_parsing(void) {
    RawFrame f;
    uint8_t mouseFrame[] = {0x57, 0xAB, 0x02, 0x01, 0x50, 0x30, 0x05};

    mockSerial.addData(mouseFrame, sizeof(mouseFrame));
    bool result = readRawFrame_test(f);

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_UINT8(0x02, f.type);
    TEST_ASSERT_EQUAL_UINT8(0x01, f.data[3]);
    TEST_ASSERT_EQUAL_UINT8(0x50, f.data[4]);
    TEST_ASSERT_EQUAL_UINT8(0x30, f.data[5]);
}

void test_03_keepalive_ignored(void) {
    RawFrame f;
    uint8_t keepaliveFrame[] = {0x57, 0xAB, 0x80, 0x00};

    mockSerial.addData(keepaliveFrame, sizeof(keepaliveFrame));
    bool result = readRawFrame_test(f);

    TEST_ASSERT_FALSE(result);
}

void test_04_frame_with_garbage_prefix(void) {
    RawFrame f;
    uint8_t garbageAndFrame[] = {0xFF, 0xFF, 0x57, 0xAB, 0x01, 0x00, 0x00, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09};

    mockSerial.addData(garbageAndFrame, sizeof(garbageAndFrame));
    bool result = readRawFrame_test(f);

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_UINT8(0x01, f.type);
}

void test_05_negative_mouse_values(void) {
    RawFrame f;
    uint8_t mouseFrame[] = {0x57, 0xAB, 0x02, 0x01, 0xCE, 0xE2, 0x00};

    mockSerial.addData(mouseFrame, sizeof(mouseFrame));
    bool result = readRawFrame_test(f);

    TEST_ASSERT_TRUE(result);
    int8_t dx = (int8_t)f.data[4];
    int8_t dy = (int8_t)f.data[5];
    TEST_ASSERT_EQUAL_INT8(-50, dx);
    TEST_ASSERT_EQUAL_INT8(-30, dy);
}

void test_06_multiple_frames_sequence(void) {
    RawFrame f1, f2;
    uint8_t keyboardFrame[] = {0x57, 0xAB, 0x01, 0x00, 0x00, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09};
    uint8_t mouseFrame[] = {0x57, 0xAB, 0x02, 0x01, 0x50, 0x30, 0x05};

    mockSerial.addData(keyboardFrame, sizeof(keyboardFrame));
    mockSerial.addData(mouseFrame, sizeof(mouseFrame));

    bool result1 = readRawFrame_test(f1);
    bool result2 = readRawFrame_test(f2);

    TEST_ASSERT_TRUE(result1);
    TEST_ASSERT_EQUAL_UINT8(0x01, f1.type);
    TEST_ASSERT_TRUE(result2);
    TEST_ASSERT_EQUAL_UINT8(0x02, f2.type);
}

// ============ MOUSE ACCUMULATION TESTS ============

void test_10_mouse_accumulation_simple(void) {
    int accumX = 0;
    int accumY = 0;

    accumX += 30;
    accumY += 20;

    TEST_ASSERT_EQUAL_INT(30, accumX);
    TEST_ASSERT_EQUAL_INT(20, accumY);
}

void test_11_mouse_constrain_exceeds_max(void) {
    int accumX = 150;
    int sendX = constrain_val(accumX, -127, 127);

    TEST_ASSERT_EQUAL_INT(127, sendX);
}

void test_12_mouse_constrain_exceeds_min(void) {
    int accumX = -150;
    int sendX = constrain_val(accumX, -127, 127);

    TEST_ASSERT_EQUAL_INT(-127, sendX);
}

void test_13_mouse_accumulation_and_subtraction(void) {
    int accumX = 0;

    accumX += 180;
    int sendX = constrain_val(accumX, -127, 127);
    TEST_ASSERT_EQUAL_INT(127, sendX);

    accumX -= sendX;
    TEST_ASSERT_EQUAL_INT(53, accumX);
}

void test_14_mouse_wheel_accumulation(void) {
    int accumWheel = 0;

    accumWheel += 3;
    accumWheel += 2;

    TEST_ASSERT_EQUAL_INT(5, accumWheel);
}

void test_15_mouse_no_movement(void) {
    int accumX = 0;
    int accumY = 0;
    int accumWheel = 0;

    bool shouldSend = (accumX != 0 || accumY != 0 || accumWheel != 0);
    TEST_ASSERT_FALSE(shouldSend);
}

void test_16_mouse_small_movements(void) {
    int accumX = 0;

    for (int i = 0; i < 10; i++) {
        accumX += 8;
    }

    TEST_ASSERT_EQUAL_INT(80, accumX);

    int sendX = constrain_val(accumX, -127, 127);
    TEST_ASSERT_EQUAL_INT(80, sendX);
}

void test_17_mouse_mixed_directions(void) {
    int accumX = 0;

    accumX += 80;
    accumX += -50;

    TEST_ASSERT_EQUAL_INT(30, accumX);
}

// ============ KEYBOARD REPORT TESTS ============

void test_20_keyboard_report_init(void) {
    KeyReport report = {0};

    TEST_ASSERT_EQUAL_UINT8(0, report.modifiers);
    TEST_ASSERT_EQUAL_UINT8(0, report.reserved);
    TEST_ASSERT_EQUAL_UINT8(0, report.keys[0]);
}

void test_21_keyboard_report_single_key(void) {
    KeyReport report = {0};
    report.keys[0] = 0x04;

    TEST_ASSERT_EQUAL_UINT8(0x04, report.keys[0]);
    TEST_ASSERT_EQUAL_UINT8(0, report.keys[1]);
}

void test_22_keyboard_report_with_modifier(void) {
    KeyReport report = {0};
    report.modifiers = 0x02;  // Shift
    report.keys[0] = 0x04;    // A

    TEST_ASSERT_EQUAL_UINT8(0x02, report.modifiers);
    TEST_ASSERT_EQUAL_UINT8(0x04, report.keys[0]);
}

void test_23_keyboard_report_6kro(void) {
    KeyReport report = {0};
    for (int i = 0; i < 6; i++) {
        report.keys[i] = 0x04 + i;
    }

    for (int i = 0; i < 6; i++) {
        TEST_ASSERT_EQUAL_UINT8(0x04 + i, report.keys[i]);
    }
}

void test_24_keyboard_report_from_raw_frame(void) {
    uint8_t rawData[11] = {0x57, 0xAB, 0x01, 0x02, 0x00, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09};

    KeyReport report = {0};
    report.modifiers = rawData[3];
    report.reserved = rawData[4];

    for (int i = 0; i < 6; i++) {
        report.keys[i] = rawData[5 + i];
    }

    TEST_ASSERT_EQUAL_UINT8(0x02, report.modifiers);
    TEST_ASSERT_EQUAL_UINT8(0x04, report.keys[0]);
    TEST_ASSERT_EQUAL_UINT8(0x09, report.keys[5]);
}

void test_25_keyboard_report_japanese_extended_keys(void) {
    KeyReport report = {0};

    report.keys[0] = 0x8B;
    report.keys[1] = 0x8A;

    TEST_ASSERT_EQUAL_UINT8(0x8B, report.keys[0]);
    TEST_ASSERT_EQUAL_UINT8(0x8A, report.keys[1]);
}

void test_26_keyboard_report_size(void) {
    KeyReport report = {0};

    TEST_ASSERT_EQUAL_INT(8, sizeof(KeyReport));
}

// ============ UNITY TEST RUNNER MAIN ============
int main(int argc, char *argv[]) {
    UNITY_BEGIN();
    
    // Frame parser tests
    RUN_TEST(test_01_keyboard_frame_parsing);
    RUN_TEST(test_02_mouse_frame_parsing);
    RUN_TEST(test_03_keepalive_ignored);
    RUN_TEST(test_04_frame_with_garbage_prefix);
    RUN_TEST(test_05_negative_mouse_values);
    RUN_TEST(test_06_multiple_frames_sequence);
    
    // Mouse accumulation tests
    RUN_TEST(test_10_mouse_accumulation_simple);
    RUN_TEST(test_11_mouse_constrain_exceeds_max);
    RUN_TEST(test_12_mouse_constrain_exceeds_min);
    RUN_TEST(test_13_mouse_accumulation_and_subtraction);
    RUN_TEST(test_14_mouse_wheel_accumulation);
    RUN_TEST(test_15_mouse_no_movement);
    RUN_TEST(test_16_mouse_small_movements);
    RUN_TEST(test_17_mouse_mixed_directions);
    
    // Keyboard report tests
    RUN_TEST(test_20_keyboard_report_init);
    RUN_TEST(test_21_keyboard_report_single_key);
    RUN_TEST(test_22_keyboard_report_with_modifier);
    RUN_TEST(test_23_keyboard_report_6kro);
    RUN_TEST(test_24_keyboard_report_from_raw_frame);
    RUN_TEST(test_25_keyboard_report_japanese_extended_keys);
    RUN_TEST(test_26_keyboard_report_size);
    
    return UNITY_END();
}
