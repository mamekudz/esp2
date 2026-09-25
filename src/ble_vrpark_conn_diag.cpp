#include "ble_vrpark_conn_diag.h"

#include "ble_scan_diag.h"

#include <NimBLEDevice.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static NimBLEClient *g_vr_client = nullptr;
static volatile bool g_vr_saw_passkey = false;
static volatile bool g_vr_saw_confirm = false;
static volatile bool g_vr_saw_auth = false;
static volatile bool g_vr_auth_enc = false;
static volatile bool g_vr_auth_bond = false;
static volatile bool g_vr_auth_authed = false;
static volatile int g_vr_last_disconnect_rc = 0;

static void vr_log(const char *msg) { Serial.printf("[VRPARK] %s\n", msg); }

static void vr_logf(const char *fmt, ...) {
  char buf[240];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  Serial.printf("[VRPARK] %s\n", buf);
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

static const char *addr_type_str(uint8_t t) {
  switch (t) {
    case 0:
      return "public";
    case 1:
      return "random";
    case 2:
      return "rpa_public_identity";
    case 3:
      return "rpa_random_identity";
    default:
      return "unknown";
  }
}

static const char *adv_type_str(uint8_t t) {
  // BLE advertising event types (legacy)
  switch (t) {
    case 0:
      return "ADV_IND(connectable-scannable)";
    case 1:
      return "ADV_DIRECT_IND_HIGH";
    case 2:
      return "ADV_SCAN_IND(scannable-nonconn)";
    case 3:
      return "ADV_NONCONN_IND(non-connectable)";
    case 4:
      return "ADV_DIRECT_IND_LOW";
    default:
      return "other/unknown";
  }
}

static void flags_decode(uint8_t f, char *out, size_t sz) {
  size_t n = 0;
  n += (size_t)snprintf(out + n, sz - n, "0x%02X", f);
  if (f & 0x01) n += (size_t)snprintf(out + n, sz - n, "|LE_Limited_Discoverable");
  if (f & 0x02) n += (size_t)snprintf(out + n, sz - n, "|LE_General_Discoverable");
  if (f & 0x04) n += (size_t)snprintf(out + n, sz - n, "|BR_EDR_Not_Supported");
  if (f & 0x08) n += (size_t)snprintf(out + n, sz - n, "|Simultaneous_LE_BR_EDR_Ctrl");
  if (f & 0x10) n += (size_t)snprintf(out + n, sz - n, "|Simultaneous_LE_BR_EDR_Host");
  (void)n;
}

static const char *hci_status_str(int hci) {
  switch (hci) {
    case 0x00:
      return "Success";
    case 0x02:
      return "Unknown Connection Identifier";
    case 0x05:
      return "Authentication Failure";
    case 0x06:
      return "Pin or Key Missing";
    case 0x08:
      return "Connection Timeout";
    case 0x09:
      return "Connection Limit Exceeded";
    case 0x0B:
      return "ACL Connection Already Exists";
    case 0x0C:
      return "Command Disallowed";
    case 0x13:
      return "Remote User Terminated Connection";
    case 0x14:
      return "Remote Device Terminated Connection - Low Resources";
    case 0x15:
      return "Remote Device Terminated Connection - Power Off";
    case 0x16:
      return "Connection Terminated by Local Host";
    case 0x1A:
      return "Unsupported Remote Feature";
    case 0x1F:
      return "Unspecified Error";
    case 0x22:
      return "LMP Response Timeout / LL Response Timeout";
    case 0x29:
      return "Pairing With Unit Key Not Supported";
    case 0x3B:
      return "Unacceptable Connection Parameters";
    case 0x3D:
      return "Connection Rejected due to Security Reasons";
    case 0x3E:
      return "Connection Failed to be Established";
    case 0x3F:
      return "MAC Connection Failed";
    default:
      return "see HCI spec";
  }
}

static void decode_rc(int rc, int *hci_out, char *rc_text, size_t rc_sz, char *hci_text,
                      size_t hci_sz) {
  *hci_out = -1;
  const char *nim = NimBLEUtils::returnCodeToString(rc);
  if (nim && nim[0]) {
    snprintf(rc_text, rc_sz, "%s", nim);
  } else {
    snprintf(rc_text, rc_sz, "rc=%d", rc);
  }

  if (rc >= BLE_HS_ERR_HCI_BASE && rc < (BLE_HS_ERR_HCI_BASE + 0x100)) {
    *hci_out = rc - BLE_HS_ERR_HCI_BASE;
    snprintf(hci_text, hci_sz, "0x%02X (%s)", *hci_out, hci_status_str(*hci_out));
  } else {
    snprintf(hci_text, hci_sz, "n/a");
  }
}

static bool name_match(const char *name, const char *substr) {
  if (!name || !substr) return false;
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

class VrParkClientCallbacks : public NimBLEClientCallbacks {
 public:
  void onConnect(NimBLEClient *pClient) override {
    (void)pClient;
    vr_log("event onConnect");
  }
  void onDisconnect(NimBLEClient *pClient) override {
    (void)pClient;
    // NimBLE 1.4 does not pass reason here; getLastError may hold it.
    const int err = pClient ? pClient->getLastError() : 0;
    g_vr_last_disconnect_rc = err;
    int hci = -1;
    char rc_text[80];
    char hci_text[64];
    decode_rc(err, &hci, rc_text, sizeof(rc_text), hci_text, sizeof(hci_text));
    vr_logf("event onDisconnect lastError=%d (%s) hci=%s", err, rc_text, hci_text);
  }
  bool onConnParamsUpdateRequest(NimBLEClient *pClient,
                                 const ble_gap_upd_params *params) override {
    (void)pClient;
    vr_logf("event connParamsUpdateReq itvl=%u..%u lat=%u to=%u — ACCEPT",
            (unsigned)params->itvl_min, (unsigned)params->itvl_max,
            (unsigned)params->latency, (unsigned)params->supervision_timeout);
    return true;
  }
  uint32_t onPassKeyRequest() override {
    g_vr_saw_passkey = true;
    vr_log("event onPassKeyRequest — NO PIN configured (returning 0)");
    return 0;
  }
  void onAuthenticationComplete(ble_gap_conn_desc *desc) override {
    g_vr_saw_auth = true;
    if (!desc) {
      vr_log("event onAuthenticationComplete desc=null");
      return;
    }
    g_vr_auth_enc = desc->sec_state.encrypted;
    g_vr_auth_bond = desc->sec_state.bonded;
    g_vr_auth_authed = desc->sec_state.authenticated;
    vr_logf("event onAuthenticationComplete enc=%u bonded=%u authed=%u key_size=%u",
            (unsigned)desc->sec_state.encrypted, (unsigned)desc->sec_state.bonded,
            (unsigned)desc->sec_state.authenticated, (unsigned)desc->sec_state.key_size);
  }
  bool onConfirmPIN(uint32_t pin) override {
    g_vr_saw_confirm = true;
    vr_logf("event onConfirmPIN pin=%lu — ACCEPT", (unsigned long)pin);
    return true;
  }
};

static VrParkClientCallbacks g_vr_cb;

/** Capture rich advertisement fields while scanning for VR PARK. */
static bool capture_vrpark_advertisement(VrParkConnDiag *out, uint32_t duration_sec) {
  NimBLEDevice::getScan()->stop();
  delay(30);

  NimBLEScan *scan = NimBLEDevice::getScan();
  scan->setActiveScan(true);
  // ble_scan_diag_init() sets MaxResults=0 (callback-only). Restore briefly
  // so NimBLEScanResults retains devices for advertisement field capture.
  scan->setMaxResults(64);
  scan->clearResults();

  vr_logf("scan start duration=%lus (active)", (unsigned long)duration_sec);
  NimBLEScanResults results = scan->start(duration_sec, false);

  bool found = false;
  const int n = results.getCount();
  vr_logf("scan done raw_results=%d", n);

  for (int i = 0; i < n; i++) {
    NimBLEAdvertisedDevice d = results.getDevice(i);
    if (!d.haveName() || !name_match(d.getName().c_str(), "VR PARK")) {
      continue;
    }
    found = true;

    strncpy(out->name, d.getName().c_str(), sizeof(out->name) - 1);
    strncpy(out->address, d.getAddress().toString().c_str(), sizeof(out->address) - 1);
    out->addr_type = (uint8_t)d.getAddressType();
    out->rssi = d.getRSSI();
    out->adv_flags = d.getAdvFlags();
    out->adv_type = d.getAdvType();
    out->connectable = d.isConnectable();
    out->appearance = d.haveAppearance() ? d.getAppearance() : 0;

    out->service_uuids[0] = '\0';
    const uint8_t ucount = d.getServiceUUIDCount();
    size_t un = 0;
    for (uint8_t u = 0; u < ucount; u++) {
      const std::string su = d.getServiceUUID(u).toString();
      if (un > 0 && un + 1 < sizeof(out->service_uuids)) {
        out->service_uuids[un++] = ',';
        out->service_uuids[un] = '\0';
      }
      un += (size_t)snprintf(out->service_uuids + un, sizeof(out->service_uuids) - un, "%s",
                             su.c_str());
    }

    out->mfg_data_hex[0] = '\0';
    if (d.haveManufacturerData()) {
      std::string md = d.getManufacturerData();
      bytes_to_hex(reinterpret_cast<const uint8_t *>(md.data()), md.size(),
                   out->mfg_data_hex, sizeof(out->mfg_data_hex));
    }

    char flags_buf[96];
    flags_decode(out->adv_flags, flags_buf, sizeof(flags_buf));

    vr_log("=== advertisement snapshot ===");
    vr_logf("name=%s", out->name);
    vr_logf("address=%s", out->address);
    vr_logf("addressType=%u (%s)", (unsigned)out->addr_type, addr_type_str(out->addr_type));
    vr_logf("RSSI=%d", (int)out->rssi);
    vr_logf("advType=%u (%s)", (unsigned)out->adv_type, adv_type_str(out->adv_type));
    vr_logf("connectable=%s", out->connectable ? "YES" : "NO");
    vr_logf("advFlags=%s", flags_buf);
    vr_logf("appearance=0x%04X", (unsigned)out->appearance);
    vr_logf("serviceUUIDs=%s", out->service_uuids[0] ? out->service_uuids : "(none)");
    vr_logf("manufacturerData=%s", out->mfg_data_hex[0] ? out->mfg_data_hex : "(none)");
    if (d.haveTXPower()) {
      vr_logf("txPower=%d", (int)d.getTXPower());
    }
    break;
  }

  scan->clearResults();
  scan->setMaxResults(0); // restore callback-only mode for BLE scanner UI
  out->found = found;
  return found;
}

bool ble_vrpark_conn_diag_run(VrParkConnDiag *out) {
  if (!ble_scan_diag_ready() || !out) {
    Serial.println("[VRPARK][FAIL] prerequisites");
    return false;
  }
  memset(out, 0, sizeof(*out));
  out->hci_status = -1;
  out->connect_rc = -1;
  g_vr_saw_passkey = false;
  g_vr_saw_confirm = false;
  g_vr_saw_auth = false;
  g_vr_auth_enc = false;
  g_vr_auth_bond = false;
  g_vr_auth_authed = false;
  g_vr_last_disconnect_rc = 0;

  vr_log("connect diagnosis START (single attempt, no blind retry)");

  // Security policy: NoInputNoOutput, bonding allowed — log if peer demands more.
  NimBLEDevice::setSecurityIOCap(ESP_IO_CAP_NONE);
  NimBLEDevice::setSecurityAuth(true /*bonding*/, false /*mitm*/, true /*sc*/);

  if (!capture_vrpark_advertisement(out, 12)) {
    vr_log("FAIL: VR PARK not in scan");
    snprintf(out->summary, sizeof(out->summary), "not in scan");
    return false;
  }

  if (!out->connectable) {
    vr_log("NOTE: advertisement reports non-connectable — connect will likely fail");
  }

  if (g_vr_client == nullptr) {
    g_vr_client = NimBLEDevice::createClient();
    g_vr_client->setClientCallbacks(&g_vr_cb, false);
    g_vr_client->setConnectTimeout(20);
    // Default-ish BLE params; log them.
    g_vr_client->setConnectionParams(12, 24, 0, 200);
  }
  if (g_vr_client->isConnected()) {
    g_vr_client->disconnect();
    delay(300);
  }

  NimBLEAddress addr(out->address, out->addr_type);
  vr_log("connect start");
  vr_logf("addressType=%u (%s)", (unsigned)out->addr_type, addr_type_str(out->addr_type));
  vr_logf("peer=%s connectable=%s", out->address, out->connectable ? "YES" : "NO");
  vr_log("security: IOCap=NONE bonding=1 mitm=0 sc=1");

  const bool ok = g_vr_client->connect(addr, true);
  const int rc = g_vr_client->getLastError();
  out->connect_rc = rc;
  decode_rc(rc, &out->hci_status, out->rc_text, sizeof(out->rc_text), out->hci_text,
            sizeof(out->hci_text));

  if (!ok) {
    out->connect_ok = false;
    vr_logf("connect FAIL reason=0x%X (%s)", rc, out->rc_text);
    if (out->hci_status >= 0) {
      vr_logf("HCI status=%s", out->hci_text);
    }
    if (g_vr_last_disconnect_rc != 0 && g_vr_last_disconnect_rc != rc) {
      int hci2 = -1;
      char t1[80], t2[64];
      decode_rc(g_vr_last_disconnect_rc, &hci2, t1, sizeof(t1), t2, sizeof(t2));
      vr_logf("disconnect side-channel lastError=%d (%s) hci=%s", g_vr_last_disconnect_rc, t1,
              t2);
    }
    vr_logf("security events: passkey=%u confirmPIN=%u authComplete=%u",
            (unsigned)g_vr_saw_passkey, (unsigned)g_vr_saw_confirm, (unsigned)g_vr_saw_auth);
    snprintf(out->summary, sizeof(out->summary), "connect FAIL rc=%d", rc);
    vr_log("diagnosis STOP — no GATT/HID (connection not established)");
    out->saw_passkey_request = g_vr_saw_passkey;
    out->saw_confirm_pin = g_vr_saw_confirm;
    out->saw_auth_complete = g_vr_saw_auth;
    return false;
  }

  out->connect_ok = true;
  vr_log("connect OK");

  // MTU (NimBLE starts exchange during connect)
  delay(200);
  out->mtu = g_vr_client->getMTU();
  out->mtu_ok = out->mtu > 0;
  vr_logf("MTU=%u", (unsigned)out->mtu);

  NimBLEConnInfo info = g_vr_client->getConnInfo();
  vr_logf("connInfo encrypted=%u bonded=%u", (unsigned)info.isEncrypted(),
          (unsigned)info.isBonded());

  // Optional: try secureConnection once and log result — does not parse HID.
  vr_log("secureConnection attempt (pairing/bonding probe)");
  const bool sec = g_vr_client->secureConnection();
  out->secured = sec;
  const int sec_rc = g_vr_client->getLastError();
  int sec_hci = -1;
  char sec_rc_text[80];
  char sec_hci_text[64];
  decode_rc(sec_rc, &sec_hci, sec_rc_text, sizeof(sec_rc_text), sec_hci_text,
            sizeof(sec_hci_text));
  vr_logf("secureConnection %s rc=%d (%s) hci=%s", sec ? "OK" : "FAIL", sec_rc, sec_rc_text,
          sec_hci_text);
  vr_logf("security events: passkey=%u confirmPIN=%u authComplete=%u enc=%u bond=%u authed=%u",
          (unsigned)g_vr_saw_passkey, (unsigned)g_vr_saw_confirm, (unsigned)g_vr_saw_auth,
          (unsigned)g_vr_auth_enc, (unsigned)g_vr_auth_bond, (unsigned)g_vr_auth_authed);

  out->saw_passkey_request = g_vr_saw_passkey;
  out->saw_confirm_pin = g_vr_saw_confirm;
  out->saw_auth_complete = g_vr_saw_auth;
  out->auth_encrypted = g_vr_auth_enc;
  out->auth_bonded = g_vr_auth_bond;
  out->auth_authenticated = g_vr_auth_authed;

  // Stay connected briefly so serial can settle; do NOT enumerate GATT/HID.
  vr_log("STABLE LINK probe done — STOP before GATT/HID enumeration");
  snprintf(out->summary, sizeof(out->summary), "connect OK mtu=%u sec=%d", (unsigned)out->mtu,
           (int)sec);

  // Disconnect cleanly so next manual RE-DIAG can retry.
  delay(100);
  g_vr_client->disconnect();
  delay(200);
  return true;
}
