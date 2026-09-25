#pragma once

#include <Arduino.h>
#include <stdint.h>

#include "ble_scan_diag.h"

enum class BleHidUsageClass : uint8_t {
  Unknown = 0,
  GamePad,
  Joystick,
  Keyboard,
  Mouse,
  ConsumerControl,
  Combination,
  Proprietary,
};

struct BleGattInspectResult {
  bool connected;
  bool has_hid_service;      // 0x1812
  bool has_hid_info;         // 0x2A4A
  bool has_report_map;       // 0x2A4B
  bool has_hid_control;      // 0x2A4C
  bool has_input_report;     // 0x2A4D notify/indicate
  bool has_battery;          // 0x180F
  bool has_device_info;      // 0x180A
  bool subscribed_input;
  BleHidUsageClass usage;
  char name[40];
  char address[20];
  char usage_label[32];
  uint16_t report_map_len;
  uint8_t report_map[256];
  char summary[96];
};

struct BleRawHidReport {
  bool valid;
  uint32_t seq;
  uint16_t handle;
  uint8_t len;
  uint8_t data[64];
  char hex[200];
};

/** Connect to device named like target (e.g. "VR PARK"), enumerate GATT. */
bool ble_gatt_inspect_by_name(const char *name_substr, BleGattInspectResult *out);

/** If HID input reports subscribed, copy latest notify (thread-safe). */
bool ble_gatt_inspect_latest_hid(BleRawHidReport *out);

bool ble_gatt_inspect_connected();
void ble_gatt_inspect_disconnect();
