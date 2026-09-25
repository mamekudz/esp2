#pragma once

#include <Arduino.h>
#include <stdint.h>

/** Result of a single VR PARK connection diagnosis (no HID parsing). */
struct VrParkConnDiag {
  bool found;
  bool connect_ok;
  bool secured;
  bool mtu_ok;
  uint8_t addr_type; // 0=public 1=random
  uint8_t adv_flags;
  uint8_t adv_type;
  bool connectable;
  int8_t rssi;
  int connect_rc;          // NimBLE host rc / getLastError()
  int hci_status;          // HCI status if rc is HCI-based, else -1
  uint16_t mtu;
  uint16_t appearance;
  char name[40];
  char address[20];
  char service_uuids[128];
  char mfg_data_hex[64];
  char rc_text[80];
  char hci_text[64];
  char summary[96];
  bool saw_passkey_request;
  bool saw_confirm_pin;
  bool saw_auth_complete;
  bool auth_encrypted;
  bool auth_bonded;
  bool auth_authenticated;
};

/**
 * One-shot VR PARK connection diagnosis:
 * scan once → log advertisement → attempt connect → log fail reason → STOP.
 * Does NOT enumerate GATT / HID.
 */
bool ble_vrpark_conn_diag_run(VrParkConnDiag *out);
