#pragma once

#include <Arduino.h>
#include <stdint.h>

// Temporary BLE discovery diagnostic (Nimbus feasibility).
// Max devices kept for AMOLED list + serial report.

static constexpr int BLE_SCAN_MAX_HITS = 16;
static constexpr uint32_t BLE_SCAN_DURATION_SEC = 15;

struct BleScanHit {
  char name[40];
  char address[20];
  uint8_t addr_type; // 0=public, 1=random, 0xFF=unknown
  int8_t rssi;
  char service_uuids[96];
  char mfg_data_hex[48];
  bool has_name;
};

struct BleScanReport {
  BleScanHit hits[BLE_SCAN_MAX_HITS];
  int count;
  uint32_t duration_ms;
  bool ok;
};

/** Init NimBLE stack. Call once after hardware bring-up. Logs [BT]/[BLE]. */
bool ble_scan_diag_init();

/** Blocking active scan for ~duration_sec. Fills report (newest/highest RSSI kept). */
bool ble_scan_diag_run(BleScanReport *out, uint32_t duration_sec = BLE_SCAN_DURATION_SEC);

bool ble_scan_diag_ready();
