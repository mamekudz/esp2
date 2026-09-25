#include "ble_gatt_inspect.h"

#include <NimBLEDevice.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <vector>

static constexpr uint16_t UUID_HID_SERVICE = 0x1812;
static constexpr uint16_t UUID_BATTERY = 0x180F;
static constexpr uint16_t UUID_DEVICE_INFO = 0x180A;
static constexpr uint16_t UUID_HID_INFO = 0x2A4A;
static constexpr uint16_t UUID_REPORT_MAP = 0x2A4B;
static constexpr uint16_t UUID_HID_CONTROL = 0x2A4C;
static constexpr uint16_t UUID_REPORT = 0x2A4D;
static constexpr uint16_t UUID_PROTOCOL_MODE = 0x2A4E;
static constexpr uint16_t UUID_BATTERY_LEVEL = 0x2A19;

static NimBLEClient *g_client = nullptr;
static bool g_connected = false;
static BleRawHidReport g_latest_hid;
static portMUX_TYPE g_hid_mux = portMUX_INITIALIZER_UNLOCKED;
static uint32_t g_hid_seq = 0;

static void gatt_log(const char *msg) { Serial.printf("[BLE] %s\n", msg); }

static void gatt_logf(const char *fmt, ...) {
  char buf[220];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  Serial.printf("[BLE] %s\n", buf);
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

static const char *props_str(NimBLERemoteCharacteristic *ch, char *buf, size_t sz) {
  buf[0] = '\0';
  if (!ch) {
    snprintf(buf, sz, "?");
    return buf;
  }
  size_t n = 0;
  auto add = [&](const char *s) {
    if (n > 0 && n + 1 < sz) {
      buf[n++] = '|';
      buf[n] = '\0';
    }
    n += (size_t)snprintf(buf + n, sz - n, "%s", s);
  };
  if (ch->canRead()) add("READ");
  if (ch->canWrite()) add("WRITE");
  if (ch->canWriteNoResponse()) add("WRITE_NR");
  if (ch->canNotify()) add("NOTIFY");
  if (ch->canIndicate()) add("INDICATE");
  if (buf[0] == '\0') snprintf(buf, sz, "none");
  return buf;
}

/** Minimal HID Report Map usage-page sniff (not a full parser). */
static BleHidUsageClass decode_report_map_usage(const uint8_t *map, uint16_t len,
                                               char *label, size_t label_sz) {
  bool gamepad = false, joystick = false, keyboard = false, mouse = false,
       consumer = false;
  uint8_t usage_page = 0;
  for (uint16_t i = 0; i < len;) {
    uint8_t b = map[i++];
    uint8_t size = b & 0x03;
    if (size == 3) size = 4;
    uint8_t type = (b >> 2) & 0x03;
    uint8_t tag = (b >> 4) & 0x0F;
    uint32_t data = 0;
    for (uint8_t s = 0; s < size && i < len; s++) {
      data |= ((uint32_t)map[i++]) << (8 * s);
    }
    if (type == 1 && tag == 0 && size >= 1) {
      usage_page = (uint8_t)(data & 0xFF); // Usage Page
    } else if (type == 2 && tag == 0 && size >= 1) {
      uint8_t usage = (uint8_t)(data & 0xFF);
      if (usage_page == 0x01) {
        if (usage == 0x02) mouse = true;
        if (usage == 0x04) joystick = true;
        if (usage == 0x05) gamepad = true;
        if (usage == 0x06) keyboard = true;
      } else if (usage_page == 0x07) {
        keyboard = true;
      } else if (usage_page == 0x0C) {
        consumer = true;
      }
    }
  }

  const int kinds =
      (int)gamepad + (int)joystick + (int)keyboard + (int)mouse + (int)consumer;
  BleHidUsageClass cls = BleHidUsageClass::Unknown;
  if (kinds >= 2) {
    cls = BleHidUsageClass::Combination;
    snprintf(label, label_sz, "combination");
  } else if (gamepad) {
    cls = BleHidUsageClass::GamePad;
    snprintf(label, label_sz, "gamepad");
  } else if (joystick) {
    cls = BleHidUsageClass::Joystick;
    snprintf(label, label_sz, "joystick");
  } else if (keyboard) {
    cls = BleHidUsageClass::Keyboard;
    snprintf(label, label_sz, "keyboard");
  } else if (mouse) {
    cls = BleHidUsageClass::Mouse;
    snprintf(label, label_sz, "mouse");
  } else if (consumer) {
    cls = BleHidUsageClass::ConsumerControl;
    snprintf(label, label_sz, "consumer");
  } else if (len > 0) {
    cls = BleHidUsageClass::Proprietary;
    snprintf(label, label_sz, "proprietary/unknown-hid");
  } else {
    snprintf(label, label_sz, "unknown");
  }
  return cls;
}

static void notifyCB(NimBLERemoteCharacteristic *pChr, uint8_t *pData, size_t length,
                     bool isNotify) {
  (void)isNotify;
  if (pData == nullptr || length == 0) {
    return;
  }
  BleRawHidReport r;
  memset(&r, 0, sizeof(r));
  r.valid = true;
  r.seq = ++g_hid_seq;
  r.handle = pChr ? pChr->getHandle() : 0;
  r.len = (uint8_t)((length > sizeof(r.data)) ? sizeof(r.data) : length);
  memcpy(r.data, pData, r.len);
  bytes_to_hex(r.data, r.len, r.hex, sizeof(r.hex));

  portENTER_CRITICAL(&g_hid_mux);
  g_latest_hid = r;
  portEXIT_CRITICAL(&g_hid_mux);

  Serial.printf("[HID-RAW] seq=%lu handle=0x%04X len=%u data=%s\n",
                (unsigned long)r.seq, (unsigned)r.handle, (unsigned)r.len, r.hex);
}

class InspectClientCallbacks : public NimBLEClientCallbacks {
 public:
  void onConnect(NimBLEClient *pClient) override {
    (void)pClient;
    g_connected = true;
    gatt_log("connected");
  }
  void onDisconnect(NimBLEClient *pClient) override {
    (void)pClient;
    g_connected = false;
    gatt_log("disconnected");
  }
};

static InspectClientCallbacks g_client_cb;

static bool name_match(const char *name, const char *substr) {
  if (!name || !substr) return false;
  // case-insensitive substring
  const size_t nlen = strlen(name);
  const size_t slen = strlen(substr);
  if (slen == 0 || nlen < slen) return false;
  for (size_t i = 0; i + slen <= nlen; i++) {
    size_t j = 0;
    for (; j < slen; j++) {
      char a = name[i + j];
      char b = substr[j];
      if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
      if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
      if (a != b) break;
    }
    if (j == slen) return true;
  }
  return false;
}

bool ble_gatt_inspect_connected() { return g_connected && g_client && g_client->isConnected(); }

void ble_gatt_inspect_disconnect() {
  if (g_client && g_client->isConnected()) {
    g_client->disconnect();
  }
  g_connected = false;
}

bool ble_gatt_inspect_latest_hid(BleRawHidReport *out) {
  if (!out) return false;
  portENTER_CRITICAL(&g_hid_mux);
  *out = g_latest_hid;
  portEXIT_CRITICAL(&g_hid_mux);
  return out->valid;
}

bool ble_gatt_inspect_by_name(const char *name_substr, BleGattInspectResult *out) {
  if (!ble_scan_diag_ready() || !name_substr || !out) {
    Serial.println("[BT][FAIL] inspect prerequisites");
    return false;
  }
  memset(out, 0, sizeof(*out));
  strncpy(out->name, name_substr, sizeof(out->name) - 1);
  snprintf(out->usage_label, sizeof(out->usage_label), "unknown");

  // Stop any scan before connecting
  NimBLEDevice::getScan()->stop();
  delay(50);

  // Fresh short scan(s) to locate address — controller may wake slowly
  gatt_logf("looking for '%s'", name_substr);
  BleScanReport scan;
  const BleScanHit *target = nullptr;
  for (int attempt = 1; attempt <= 3 && !target; attempt++) {
    gatt_logf("discover attempt %d/3", attempt);
    if (!ble_scan_diag_run(&scan, 12)) {
      Serial.println("[BT][FAIL] rescan for target");
      snprintf(out->summary, sizeof(out->summary), "scan failed");
      return false;
    }
    for (int i = 0; i < scan.count; i++) {
      if (name_match(scan.hits[i].name, name_substr)) {
        target = &scan.hits[i];
        break;
      }
    }
    if (!target) {
      gatt_logf("'%s' not in scan (%d hits)", name_substr, scan.count);
    }
  }
  if (!target) {
    gatt_logf("[FAIL] '%s' not found after retries", name_substr);
    snprintf(out->summary, sizeof(out->summary), "not found in scan");
    return false;
  }

  strncpy(out->name, target->name, sizeof(out->name) - 1);
  strncpy(out->address, target->address, sizeof(out->address) - 1);
  gatt_logf("target name=%s address=%s RSSI=%d", target->name, target->address,
            (int)target->rssi);

  if (g_client == nullptr) {
    g_client = NimBLEDevice::createClient();
    g_client->setClientCallbacks(&g_client_cb, false);
    g_client->setConnectTimeout(15);
  }
  if (g_client->isConnected()) {
    g_client->disconnect();
    delay(200);
  }

  NimBLEAddress addr(target->address, target->addr_type);
  gatt_log("connecting ...");
  if (!g_client->connect(addr, true)) {
    Serial.println("[BT][FAIL] connect");
    snprintf(out->summary, sizeof(out->summary), "connect failed");
    return false;
  }
  out->connected = true;
  g_connected = true;
  gatt_log("connected");

  // Optional secure connection — may be required for HID Report Map
  if (!g_client->secureConnection()) {
    gatt_log("secureConnection skipped/failed (continuing)");
  }

  gatt_log("enumerating services...");
  const std::vector<NimBLERemoteService *> *services = g_client->getServices(true);
  if (services == nullptr) {
    Serial.println("[BT][FAIL] getServices null");
    snprintf(out->summary, sizeof(out->summary), "no services");
    return false;
  }

  gatt_logf("service count=%u", (unsigned)services->size());
  std::vector<NimBLERemoteCharacteristic *> input_reports;

  for (auto *svc : *services) {
    if (!svc) continue;
    const std::string su = svc->getUUID().toString();
    gatt_logf("service %s", su.c_str());

    if (svc->getUUID().equals(NimBLEUUID((uint16_t)UUID_HID_SERVICE))) {
      out->has_hid_service = true;
    }
    if (svc->getUUID().equals(NimBLEUUID((uint16_t)UUID_BATTERY))) {
      out->has_battery = true;
    }
    if (svc->getUUID().equals(NimBLEUUID((uint16_t)UUID_DEVICE_INFO))) {
      out->has_device_info = true;
    }

    const std::vector<NimBLERemoteCharacteristic *> *chrs = svc->getCharacteristics(true);
    if (!chrs) continue;
    for (auto *ch : *chrs) {
      if (!ch) continue;
      char pbuf[48];
      gatt_logf("  char %s props=%s handle=0x%04X", ch->getUUID().toString().c_str(),
                props_str(ch, pbuf, sizeof(pbuf)), (unsigned)ch->getHandle());

      if (ch->getUUID().equals(NimBLEUUID((uint16_t)UUID_HID_INFO))) {
        out->has_hid_info = true;
        if (ch->canRead()) {
          std::string v = ch->readValue();
          char hex[64];
          bytes_to_hex(reinterpret_cast<const uint8_t *>(v.data()), v.size(), hex,
                       sizeof(hex));
          gatt_logf("  HID Information: %s", hex);
        }
      }
      if (ch->getUUID().equals(NimBLEUUID((uint16_t)UUID_REPORT_MAP))) {
        out->has_report_map = true;
        if (ch->canRead()) {
          std::string v = ch->readValue();
          out->report_map_len =
              (uint16_t)((v.size() > sizeof(out->report_map)) ? sizeof(out->report_map)
                                                              : v.size());
          memcpy(out->report_map, v.data(), out->report_map_len);
          char hex[200];
          bytes_to_hex(out->report_map,
                       out->report_map_len > 64 ? 64 : out->report_map_len, hex,
                       sizeof(hex));
          gatt_logf("  Report Map len=%u first64=%s%s", (unsigned)out->report_map_len,
                    hex, out->report_map_len > 64 ? "..." : "");
          out->usage = decode_report_map_usage(out->report_map, out->report_map_len,
                                               out->usage_label, sizeof(out->usage_label));
          gatt_logf("  Report Map usage class=%s", out->usage_label);
        } else {
          gatt_log("  Report Map present but not readable (auth?)");
        }
      }
      if (ch->getUUID().equals(NimBLEUUID((uint16_t)UUID_HID_CONTROL))) {
        out->has_hid_control = true;
      }
      if (ch->getUUID().equals(NimBLEUUID((uint16_t)UUID_REPORT))) {
        if (ch->canNotify() || ch->canIndicate()) {
          out->has_input_report = true;
          input_reports.push_back(ch);
        }
      }
      if (ch->getUUID().equals(NimBLEUUID((uint16_t)UUID_PROTOCOL_MODE)) && ch->canRead()) {
        std::string v = ch->readValue();
        if (!v.empty()) {
          gatt_logf("  Protocol Mode=0x%02X", (unsigned)(uint8_t)v[0]);
        }
      }
      if (ch->getUUID().equals(NimBLEUUID((uint16_t)UUID_BATTERY_LEVEL)) && ch->canRead()) {
        std::string v = ch->readValue();
        if (!v.empty()) {
          gatt_logf("  Battery Level=%u%%", (unsigned)(uint8_t)v[0]);
        }
      }

      // Note notify chars outside HID service (document only — no reverse eng).
      const bool in_hid =
          svc->getUUID().equals(NimBLEUUID((uint16_t)UUID_HID_SERVICE));
      if (!in_hid && (ch->canNotify() || ch->canIndicate())) {
        gatt_logf("  proprietary notify candidate %s", ch->getUUID().toString().c_str());
      }
    }
  }

  if (out->has_hid_service && out->has_input_report) {
    for (auto *ch : input_reports) {
      if (ch->subscribe(true, notifyCB)) {
        out->subscribed_input = true;
        gatt_logf("subscribed Input Report handle=0x%04X", (unsigned)ch->getHandle());
      } else {
        gatt_logf("[FAIL] subscribe handle=0x%04X", (unsigned)ch->getHandle());
      }
    }
  }

  if (!out->has_hid_service) {
    out->usage = BleHidUsageClass::Proprietary;
    snprintf(out->usage_label, sizeof(out->usage_label), "no-HID-service");
    snprintf(out->summary, sizeof(out->summary), "BLE OK but NOT standard HID");
    gatt_log("RESULT: no HID Service 0x1812 — proprietary BLE (stop before RE)");
  } else if (out->subscribed_input) {
    snprintf(out->summary, sizeof(out->summary), "HID OK raw notify live");
    gatt_logf("RESULT: standard BLE HID (%s) — raw reports enabled", out->usage_label);
  } else {
    snprintf(out->summary, sizeof(out->summary), "HID svc but no notify");
    gatt_log("RESULT: HID service present but input subscribe failed");
  }

  return true;
}
