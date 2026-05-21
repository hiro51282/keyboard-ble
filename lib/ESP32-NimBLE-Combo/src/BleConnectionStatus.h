#ifndef ESP32_BLE_CONNECTION_STATUS_H
#define ESP32_BLE_CONNECTION_STATUS_H
#include "sdkconfig.h"
#if defined(CONFIG_BT_ENABLED)

#include <NimBLEServer.h>
#include "NimBLECharacteristic.h"
#include "NimBLEAddress.h"

class BleConnectionStatus : public NimBLEServerCallbacks {
public:
  BleConnectionStatus(void);
  bool connected = false;
  NimBLEAddress peerAddress;
  uint16_t connHandle = 0xFFFF;
  void onConnect(NimBLEServer* pServer, ble_gap_conn_desc* desc);
  void onDisconnect(NimBLEServer* pServer);
  void onAuthenticationComplete(ble_gap_conn_desc* desc);
  NimBLECharacteristic* inputKeyboard;
  NimBLECharacteristic* outputKeyboard;
  NimBLECharacteristic* inputMouse;
};

#endif // CONFIG_BT_ENABLED
#endif // ESP32_BLE_CONNECTION_STATUS_H
