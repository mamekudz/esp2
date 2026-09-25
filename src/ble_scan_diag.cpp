#include "ble_scan_diag.h"

#include <NimBLEDevice.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static bool g_ble_ready = false;
static BleScanReport g_accum;
static portMUX_TYPE g_accum_mux = portMUX_INITIALIZER_UNLOCKED;

static void bt_log(const char *msg) { Serial.printf("[BT] %s\n", msg); }

static void bt_logf(const char *fmt, ...) {
  char buf[200];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  Serial.printf("[BT] %s\n", buf);
}

static void scan_logf(const char *fmt, ...) {
  char buf[240];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  Serial.printf("[BLE-SCAN] %s\n", buf);
}

static void bytes_to_hex(const uint8_t *data, size_t len, char *out, size_t out_sz) {
  size_t n = 0;
  out[0] = '\0';
  for (size_t i = 0; i < len && (n + 3) < out_sz; i++) {
    n += (size_t)snprintf(out + n, out_sz - n, "%02X", data[i]);
    if (i + 1 < len && (n + 2) < out_sz) {
      out[n++] = ' ';
      out[n] = '\0';
    }
  }
}

static int find_hit_index(const char *address) {
  for (int i = 0; i < g_accum.count; i++) {
    if (strncmp(g_accum.hits[i].address, address, sizeof(g_accum.hits[i].address)) ==
        0) {
      return i;
    }
  }
  return -1;
}

static void upsert_hit(const BleScanHit &hit) {
  portENTER_CRITICAL(&g_accum_mux);
  int idx = find_hit_index(hit.address);
  if (idx >= 0) {
    if (hit.rssi > g_accum.hits[idx].rssi) {
      g_accum.hits[idx].rssi = hit.rssi;
    }
    if (hit.has_name && hit.name[0]) {
      strncpy(g_accum.hits[idx].name, hit.name, sizeof(g_accum.hits[idx].name) - 1);
      g_accum.hits[idx].has_name = true;
    }
    if (hit.service_uuids[0] && !g_accum.hits[idx].service_uuids[0]) {
      strncpy(g_accum.hits[idx].service_uuids, hit.service_uuids,
              sizeof(g_accum.hits[idx].service_uuids) - 1);
    }
    if (hit.mfg_data_hex[0] && !g_accum.hits[idx].mfg_data_hex[0]) {
      strncpy(g_accum.hits[idx].mfg_data_hex, hit.mfg_data_hex,
              sizeof(g_accum.hits[idx].mfg_data_hex) - 1);
    }
  } else if (g_accum.count < BLE_SCAN_MAX_HITS) {
    g_accum.hits[g_accum.count++] = hit;
  } else {
    int weakest = 0;
    for (int i = 1; i < g_accum.count; i++) {
      if (g_accum.hits[i].rssi < g_accum.hits[weakest].rssi) {
        weakest = i;
      }
    }
    if (hit.rssi > g_accum.hits[weakest].rssi) {
      g_accum.hits[weakest] = hit;
    }
  }
  portEXIT_CRITICAL(&g_accum_mux);
}

// NimBLE-Arduino 1.4.x callbacks
class EspBracketAdvertisedCallbacks : public NimBLEAdvertisedDeviceCallbacks {
 public:
  void onResult(NimBLEAdvertisedDevice *advertisedDevice) override {
    if (advertisedDevice == nullptr) {
      return;
    }

    BleScanHit hit;
    memset(&hit, 0, sizeof(hit));

    const std::string addr = advertisedDevice->getAddress().toString();
    strncpy(hit.address, addr.c_str(), sizeof(hit.address) - 1);
    hit.addr_type = (uint8_t)advertisedDevice->getAddressType();
    hit.rssi = advertisedDevice->getRSSI();

    if (advertisedDevice->haveName()) {
      const std::string nm = advertisedDevice->getName();
      strncpy(hit.name, nm.c_str(), sizeof(hit.name) - 1);
      hit.has_name = hit.name[0] != '\0';
    } else {
      strncpy(hit.name, "(no name)", sizeof(hit.name) - 1);
      hit.has_name = false;
    }

    hit.service_uuids[0] = '\0';
    if (advertisedDevice->haveServiceUUID()) {
      const std::string u = advertisedDevice->getServiceUUID().toString();
      strncpy(hit.service_uuids, u.c_str(), sizeof(hit.service_uuids) - 1);
    }

    if (advertisedDevice->haveManufacturerData()) {
      std::string md = advertisedDevice->getManufacturerData();
      bytes_to_hex(reinterpret_cast<const uint8_t *>(md.data()), md.size(),
                   hit.mfg_data_hex, sizeof(hit.mfg_data_hex));
    }

    scan_logf("name=%s address=%s addressType=%u RSSI=%d serviceUUIDs=%s "
              "manufacturerData=%s",
              hit.name, hit.address, (unsigned)hit.addr_type, (int)hit.rssi,
              hit.service_uuids[0] ? hit.service_uuids : "-",
              hit.mfg_data_hex[0] ? hit.mfg_data_hex : "-");

    upsert_hit(hit);
  }
};

static EspBracketAdvertisedCallbacks g_adv_cb;

bool ble_scan_diag_ready() { return g_ble_ready; }

bool ble_scan_diag_init() {
  bt_log("initializing");
  if (g_ble_ready) {
    return true;
  }

  NimBLEDevice::init("");
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);

  NimBLEScan *scan = NimBLEDevice::getScan();
  if (scan == nullptr) {
    Serial.println("[BT][FAIL] getScan() null");
    return false;
  }

  scan->setAdvertisedDeviceCallbacks(&g_adv_cb, false);
  scan->setActiveScan(true);
  scan->setInterval(80);
  scan->setWindow(40);
  scan->setMaxResults(0);

  g_ble_ready = true;
  bt_log("ready (NimBLE 1.4 scanner)");
  Serial.println("[BLE-SCAN] OK");
  return true;
}

bool ble_scan_diag_run(BleScanReport *out, uint32_t duration_sec) {
  if (!g_ble_ready || out == nullptr) {
    Serial.println("[BT][FAIL] scanner not ready");
    return false;
  }

  memset(&g_accum, 0, sizeof(g_accum));
  memset(out, 0, sizeof(*out));

  bt_log("scanning");
  const uint32_t t0 = millis();

  NimBLEScan *scan = NimBLEDevice::getScan();
  NimBLEScanResults results = scan->start(duration_sec, false);
  (void)results;
  scan->clearResults();

  const uint32_t dt = millis() - t0;

  portENTER_CRITICAL(&g_accum_mux);
  *out = g_accum;
  out->duration_ms = dt;
  out->ok = true;
  portEXIT_CRITICAL(&g_accum_mux);

  bt_logf("scan done hits=%d duration_ms=%lu", out->count,
          (unsigned long)out->duration_ms);
  return true;
}
