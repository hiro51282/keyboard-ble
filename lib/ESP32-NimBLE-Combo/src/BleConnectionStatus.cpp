#include "BleConnectionStatus.h"

BleConnectionStatus::BleConnectionStatus(void) {
}

void BleConnectionStatus::onConnect(NimBLEServer* pServer, ble_gap_conn_desc* desc)
{
  this->connected = true;
  // Request shorter connection interval immediately after connect
  // 6=7.5ms, 12=15ms, 0=no slave latency, 51=510ms supervision timeout
  pServer->updateConnParams(desc->conn_handle, 6, 12, 0, 51);
}

void BleConnectionStatus::onDisconnect(NimBLEServer* pServer)
{
  this->connected = false; 
}
