#include "BleConnectionStatus.h"
#if defined(CONFIG_BT_ENABLED)
#include <NimBLEDevice.h>
#endif

BleConnectionStatus::BleConnectionStatus(void) {
}

void BleConnectionStatus::onConnect(NimBLEServer* pServer, ble_gap_conn_desc* desc)
{
  this->connected = true;
  this->peerAddress = NimBLEAddress(desc->peer_id_addr);
  this->connHandle = desc->conn_handle;

  int numBonds = NimBLEDevice::getNumBonds();
  Serial.printf("Connected: addr=%s bonds=%d bonded=%s\n",
    this->peerAddress.toString().c_str(),
    numBonds,
    NimBLEDevice::isBonded(this->peerAddress) ? "yes" : "no");
  for (int i = 0; i < numBonds; i++) {
    Serial.printf("  bond[%d] = %s\n", i, NimBLEDevice::getBondedAddress(i).toString().c_str());
  }

  // 切り替え後にホワイトリストとフィルタをリセットして通常動作に戻す
  if (NimBLEDevice::getWhiteListCount() > 0) {
    NimBLEDevice::whiteListRemove(NimBLEAddress(desc->peer_id_addr));
    pServer->getAdvertising()->setScanFilter(false, false);
  }

  // Request shorter connection interval immediately after connect
  // 6=7.5ms, 12=15ms, 0=no slave latency, 51=510ms supervision timeout
  pServer->updateConnParams(desc->conn_handle, 6, 12, 0, 51);
}

void BleConnectionStatus::onDisconnect(NimBLEServer* pServer)
{
  this->connected = false;
  this->connHandle = 0xFFFF;
}

void BleConnectionStatus::onAuthenticationComplete(ble_gap_conn_desc* desc)
{
  NimBLEAddress addr(desc->peer_id_addr);
  int numBonds = NimBLEDevice::getNumBonds();
  Serial.printf("AuthComplete: addr=%s bonded=%s bonds=%d\n",
    addr.toString().c_str(),
    NimBLEDevice::isBonded(addr) ? "yes" : "no",
    numBonds);
  for (int i = 0; i < numBonds; i++) {
    Serial.printf("  bond[%d] = %s\n", i, NimBLEDevice::getBondedAddress(i).toString().c_str());
  }
}
