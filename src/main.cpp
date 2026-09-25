#include <Arduino.h>
#include <stdarg.h>
#include <string.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <Preferences.h>
#include <Arduino_GFX_Library.h>

#include "board_pins.h"
#include "display_power.h"
#include "qmi8658_min.h"
#include "ble_scan_diag.h"
#include "ble_vrpark_conn_diag.h"

static Arduino_DataBus *bus = nullptr;
static Arduino_CO5300 *gfx = nullptr;
static DisplayPowerManager g_display_power;
static constexpr uint8_t kDisplayBrightness = 200;
static char g_ble_status_line[48] = "ready";

static bool g_display_ok = false;
static bool g_touch_ok = false;
static bool g_sd_ok = false;
static bool g_sd_persist_ok = false;
static bool g_imu_ok = false;
static bool g_ble_ok = false;
static bool g_psram_ok = false;
static bool g_flash_ok = false;

static BleScanReport g_ble_report;
static bool g_ble_screen_active = false;
static bool g_ble_scan_busy = false;
static VrParkConnDiag g_vr_diag;
static bool g_vr_diag_done = false;
static bool g_vr_auto_diag_done = false; // one deferred attempt only — no blind retry

// Touch transform (raw FT3168 -> display). Identity for Phase 1 milestone 2.
static bool g_touch_swap_xy = false;
static bool g_touch_invert_x = false;
static bool g_touch_invert_y = false;

static uint16_t g_last_disp_x = 0;
static uint16_t g_last_disp_y = 0;
static bool g_touch_down = false;

static void logf(const char *tag, const char *fmt, ...) {
  char buf[192];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  Serial.printf("[%s] %s\n", tag, buf);
}

static void report_chip() {
  logf("BOOT", "ESP][ Phase1 BLE-SCAN (Nimbus feasibility)");
  logf("CHIP", "ESP32-S3 model=%s rev=%d cores=%d",
       ESP.getChipModel(), ESP.getChipRevision(), ESP.getChipCores());
  logf("CHIP", "freq=%u MHz sdk=%s", ESP.getCpuFreqMHz(), ESP.getSdkVersion());
}

static void report_memory() {
  uint32_t flash = ESP.getFlashChipSize();
  g_flash_ok = (flash >= 16UL * 1024UL * 1024UL);
  if (g_flash_ok) {
    logf("FLASH", "%u bytes OK", flash);
  } else {
    logf("FLASH", "[FAIL] %u bytes (expected >= 16777216)", flash);
  }

  uint32_t psram = ESP.getPsramSize();
  g_psram_ok = (psram >= 8UL * 1024UL * 1024UL);
  if (g_psram_ok) {
    logf("PSRAM", "%u bytes OK", psram);
  } else if (psram > 0) {
    logf("PSRAM", "[FAIL] %u bytes (expected >= 8388608)", psram);
  } else {
    logf("PSRAM", "[FAIL] not detected");
  }

  logf("RAM", "heap_total=%u heap_free=%u heap_min=%u",
       ESP.getHeapSize(), ESP.getFreeHeap(), ESP.getMinFreeHeap());
  if (psram > 0) {
    logf("RAM", "psram_free=%u", ESP.getFreePsram());
  }
}

static bool init_display() {
#if defined(BOARD_PCB_V2)
  logf("DISPLAY", "PCB profile V2 (LCD_CS=GPIO46)");
#else
  logf("DISPLAY", "PCB profile V1 (LCD_CS=GPIO9)");
#endif

  bus = new Arduino_ESP32QSPI(
      PIN_LCD_CS, PIN_LCD_SCK, PIN_LCD_D0, PIN_LCD_D1, PIN_LCD_D2, PIN_LCD_D3);
  gfx = new Arduino_CO5300(
      bus, PIN_LCD_RST, 0 /* native portrait */,
      LCD_WIDTH, LCD_HEIGHT,
      LCD_COL_OFFSET1, LCD_ROW_OFFSET1, LCD_COL_OFFSET2, LCD_ROW_OFFSET2);

  if (!gfx->begin()) {
    logf("DISPLAY", "[FAIL] CO5300 begin() returned false");
    return false;
  }

  gfx->setBrightness(kDisplayBrightness);
  logf("DISPLAY", "CO5300 init OK");
  logf("DISPLAY", "%dx%d", gfx->width(), gfx->height());
  logf("DISPLAY", "orientation=PORTRAIT_NATIVE rotation=0");
  return true;
}

// --- AMOLED power / anti-burn-in (panel only; ESP32 stays awake) ---

static void panel_sleep_co5300() {
  if (gfx == nullptr) {
    return;
  }
  // Real controller sleep: DISPOFF + SLPIN (Arduino_CO5300::displayOff).
  gfx->displayOff();
}

static void panel_wake_co5300() {
  if (gfx == nullptr) {
    return;
  }
  // Matching wake: DISPON + SLPOUT, then restore brightness.
  gfx->displayOn();
  gfx->setBrightness(kDisplayBrightness);
}

static void draw_amoled_screensaver(uint8_t slot_index) {
  if (!g_display_ok || gfx == nullptr) {
    return;
  }
  // Mostly black; one small brand mark relocates across 8 slots.
  // Brand glyph is not an i18x UI string.
  static const int16_t kSlots[8][2] = {
      {16, 40},   {160, 70},  {40, 140}, {180, 180},
      {24, 250},  {150, 290}, {60, 340}, {120, 400},
  };
  const uint8_t slot = slot_index % 8;
  gfx->fillScreen(RGB565_BLACK);
  gfx->setTextSize(2);
  gfx->setTextColor(RGB565_DARKGREY, RGB565_BLACK);
  gfx->setCursor(kSlots[slot][0], kSlots[slot][1]);
  gfx->print("ESP][");
}

static void restore_ui_after_wake();

static void display_power_begin_after_ui() {
  DisplayPowerSettings cfg;
  cfg.screensaver = ScreensaverTimeout::Min2;
  cfg.screen_off = ScreenOffTimeout::Min5;
  cfg.screensaver_move_ms = 8000;
#if defined(DISPLAY_POWER_TEST_SHORT)
  // Lab-only short timeouts for physical AMOLED verification.
  cfg.screensaver_override_ms = 10UL * 1000UL;
  cfg.screen_off_override_ms = 20UL * 1000UL;
  cfg.screensaver_move_ms = 3000;
  logf("DISPLAY-POWER", "TEST_SHORT screensaver=10s off=20s");
#endif
  g_display_power.setSettings(cfg);
  g_display_power.begin(draw_amoled_screensaver, restore_ui_after_wake,
                        panel_sleep_co5300, panel_wake_co5300);
  logf("DISPLAY-POWER",
       "ACTIVE screensaver=%lums off=%lums",
       (unsigned long)display_power_screensaver_ms(cfg),
       (unsigned long)display_power_screen_off_ms(cfg));
}

static void draw_label(int16_t x, int16_t y, uint16_t fg, uint16_t bg, const char *text) {
  gfx->setTextColor(fg, bg);
  gfx->setCursor(x, y);
  gfx->print(text);
}

static void draw_diagnostic_screen() {
  const int16_t w = LCD_WIDTH;
  const int16_t h = LCD_HEIGHT;

  gfx->fillScreen(RGB565_BLACK);

  const int16_t band_h = 48;
  const int16_t block_w = w / 5;
  gfx->fillRect(0 * block_w, 0, block_w, band_h, RGB565_BLACK);
  gfx->fillRect(1 * block_w, 0, block_w, band_h, RGB565_WHITE);
  gfx->fillRect(2 * block_w, 0, block_w, band_h, RGB565_RED);
  gfx->fillRect(3 * block_w, 0, block_w, band_h, RGB565_GREEN);
  gfx->fillRect(4 * block_w, 0, w - 4 * block_w, band_h, RGB565_BLUE);

  gfx->setTextSize(1);
  draw_label(4, 18, RGB565_WHITE, RGB565_BLACK, "BLK");
  draw_label(block_w + 4, 18, RGB565_BLACK, RGB565_WHITE, "WHT");
  draw_label(2 * block_w + 4, 18, RGB565_WHITE, RGB565_RED, "RED");
  draw_label(3 * block_w + 4, 18, RGB565_BLACK, RGB565_GREEN, "GRN");
  draw_label(4 * block_w + 4, 18, RGB565_WHITE, RGB565_BLUE, "BLU");

  const int16_t bar_y = band_h + 8;
  const int16_t bar_h = 28;
  gfx->fillRect(8, bar_y, w - 16, bar_h, RGB565_RED);
  gfx->fillRect(8, bar_y + bar_h + 4, w - 16, bar_h, RGB565_GREEN);
  gfx->fillRect(8, bar_y + 2 * (bar_h + 4), w - 16, bar_h, RGB565_BLUE);

  gfx->drawRect(0, 0, w, h, RGB565_WHITE);
  gfx->drawRect(1, 1, w - 2, h - 2, RGB565_YELLOW);

  gfx->setTextSize(2);
  draw_label(w / 2 - 24, 150, RGB565_YELLOW, RGB565_BLACK, "TOP");
  draw_label(8, h / 2 - 8, RGB565_YELLOW, RGB565_BLACK, "L");
  draw_label(w - 24, h / 2 - 8, RGB565_YELLOW, RGB565_BLACK, "R");
  draw_label(w / 2 - 40, h - 40, RGB565_YELLOW, RGB565_BLACK, "BOTTOM");

  gfx->setTextSize(1);
  draw_label(8, h / 2 + 16, RGB565_CYAN, RGB565_BLACK, "LEFT");
  draw_label(w - 40, h / 2 + 16, RGB565_CYAN, RGB565_BLACK, "RIGHT");

  gfx->setTextSize(2);
  draw_label(36, 210, RGB565_WHITE, RGB565_BLACK, "280x456");
  gfx->setTextSize(1);
  draw_label(40, 240, RGB565_WHITE, RGB565_BLACK, "PORTRAIT_NATIVE");
  draw_label(52, 256, RGB565_WHITE, RGB565_BLACK, "ESP][ Phase 1");
  draw_label(16, 280, RGB565_GREEN, RGB565_BLACK, "IMU MILESTONE");
  draw_label(16, 296, RGB565_GREEN, RGB565_BLACK, "tilt board; serial IMU");

  logf("DISPLAY", "diagnostic pattern drawn");
}

static int i2c_scan_bus(const char *phase) {
  logf("I2C", "SDA=%d SCL=%d phase=%s", PIN_TOUCH_SDA, PIN_TOUCH_SCL, phase);
  logf("I2C", "scanning...");
  int found = 0;
  bool saw_38 = false;
  bool saw_imu = false;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    uint8_t err = Wire.endTransmission();
    if (err == 0) {
      const char *name = "";
      if (addr == FT3168_I2C_ADDR) {
        name = " = FT3168";
        saw_38 = true;
      } else if (addr == QMI8658_I2C_ADDR_H || addr == QMI8658_I2C_ADDR_L) {
        name = " = QMI8658?";
        saw_imu = true;
      } else if (addr == 0x7E) {
        name = " = (ghost/reserved)";
      }
      logf("I2C", "device 0x%02X%s", addr, name);
      found++;
    }
  }
  if (found == 0) {
    logf("I2C", "[FAIL] no devices found (%s)", phase);
  } else {
    logf("I2C", "scan done found=%d ft3168=%s qmi8658=%s", found,
         saw_38 ? "YES" : "NO", saw_imu ? "YES" : "NO");
  }
  return found;
}

static bool i2c_probe_addr(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

static bool wait_for_ft3168(uint32_t timeout_ms) {
  const uint32_t start = millis();
  while ((millis() - start) < timeout_ms) {
    if (i2c_probe_addr(FT3168_I2C_ADDR)) {
      return true;
    }
    delay(50);
  }
  return false;
}

static void touch_prepare_gpio() {
  // Rev1.1: TP_INT -> GPIO18. Keep as input with pull-up (R39 also present).
  pinMode(PIN_TP_INT, INPUT_PULLUP);
  // Do NOT drive GPIO8 as TP_RST: Rev1.1 marks R34(IO8) as NC.
  // TP_RESET is tied to LCD_RESET (GPIO21) via R35 0R.
  pinMode(8, INPUT);
  logf("TOUCH", "TP_INT=GPIO%d INPUT_PULLUP; GPIO8 left INPUT (NC on Rev1.1)",
       PIN_TP_INT);
  logf("TOUCH", "TP_RESET shared with LCD_RESET=GPIO%d (schematic R35)",
       PIN_TP_RST_SHARED_WITH_LCD);
}

static bool ft3168_write_u8(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(FT3168_I2C_ADDR);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

static bool ft3168_enter_normal_mode() {
  // DEV_MODE working mode
  if (!ft3168_write_u8(0x00, 0x00)) {
    logf("TOUCH", "[FAIL] DEV_MODE write");
    return false;
  }
  // G_MODE 0 = polling (not interrupt-trigger). Critical for continuous Wire polls
  // when INT may stay high / unused.
  if (!ft3168_write_u8(0xA4, 0x00)) {
    logf("TOUCH", "[FAIL] G_MODE write");
    return false;
  }
  // PMODE 0 = active (not monitor/hibernate)
  if (!ft3168_write_u8(0xA5, 0x00)) {
    logf("TOUCH", "[FAIL] PMODE write");
    return false;
  }
  logf("TOUCH", "mode DEV=0 G_MODE=poll PMODE=active");
  return true;
}

static void ft3168_dump_ids() {
  uint8_t regs[] = {0xA3, 0xA8, 0x9F, 0xA6};
  for (uint8_t r : regs) {
    Wire.beginTransmission(FT3168_I2C_ADDR);
    Wire.write(r);
    if (Wire.endTransmission(false) != 0) {
      logf("TOUCH", "id reg 0x%02X read fail", r);
      continue;
    }
    if (Wire.requestFrom((int)FT3168_I2C_ADDR, 1) != 1) {
      logf("TOUCH", "id reg 0x%02X empty", r);
      continue;
    }
    uint8_t v = Wire.read();
    logf("TOUCH", "id[0x%02X]=0x%02X", r, v);
  }
}

static void pulse_shared_tp_lcd_reset(uint32_t low_ms, uint32_t high_wait_ms) {
  pinMode(PIN_TP_RST_SHARED_WITH_LCD, OUTPUT);
  digitalWrite(PIN_TP_RST_SHARED_WITH_LCD, LOW);
  delay(low_ms);
  digitalWrite(PIN_TP_RST_SHARED_WITH_LCD, HIGH);
  delay(high_wait_ms);
}

static bool init_touch_after_display() {
  logf("TOUCH", "starting");
  touch_prepare_gpio();

  // LCD reset also resets FT3168 (shared net). Allow FocalTech boot time.
  const uint32_t t0 = millis();
  if (!wait_for_ft3168(4000)) {
    logf("TOUCH", "0x38 not yet visible after %lu ms — rescanning",
         (unsigned long)(millis() - t0));
    i2c_scan_bus("post-display");
  } else {
    logf("TOUCH", "0x38 ACK after %lu ms post-display",
         (unsigned long)(millis() - t0));
  }

  if (!i2c_probe_addr(FT3168_I2C_ADDR)) {
    // Shared reset also resets CO5300 — re-begin panel afterwards.
    logf("TOUCH", "shared LCD/TP reset pulse on GPIO%d",
         PIN_TP_RST_SHARED_WITH_LCD);
    pulse_shared_tp_lcd_reset(30, 500);
    Wire.begin(PIN_TOUCH_SDA, PIN_TOUCH_SCL);
    Wire.setClock(100000);
    i2c_scan_bus("after-shared-reset");

    if (gfx != nullptr) {
      if (!gfx->begin()) {
        logf("DISPLAY", "[FAIL] CO5300 re-begin after shared reset");
      } else {
        gfx->setBrightness(kDisplayBrightness);
        draw_diagnostic_screen();
        logf("DISPLAY", "re-init OK after shared TP/LCD reset");
      }
    }
  }

  if (!i2c_probe_addr(FT3168_I2C_ADDR)) {
    // One more settle window — FT3168 sometimes ACKs late after panel QSPI init.
    if (wait_for_ft3168(3000)) {
      logf("TOUCH", "0x38 appeared in late settle window");
    }
  }

  if (!i2c_probe_addr(FT3168_I2C_ADDR)) {
    logf("I2C", "[FAIL] FT3168 0x38 not detected");
    logf("TOUCH", "[FAIL] FT3168 not responding at 0x%02X", FT3168_I2C_ADDR);
    return false;
  }

  logf("I2C", "device 0x38 found");
  logf("TOUCH", "FT3168 detected at 0x38");

  if (!ft3168_enter_normal_mode()) {
    return false;
  }
  ft3168_dump_ids();

  // Prefer 300 kHz once the device is up (jaapp default).
  Wire.setClock(300000);

  logf("TOUCH", "ready");
  logf("TOUCH", "transform identity (swap=0 invX=0 invY=0) — raw==display");
  return true;
}

static bool try_recover_touch() {
  if (i2c_probe_addr(FT3168_I2C_ADDR)) {
    if (ft3168_enter_normal_mode()) {
      Wire.setClock(300000);
      logf("TOUCH", "FT3168 recovered");
      logf("TOUCH", "ready");
      return true;
    }
  }
  return false;
}

static void raw_to_display(uint16_t raw_x, uint16_t raw_y, uint16_t *disp_x, uint16_t *disp_y) {
  uint16_t x = raw_x;
  uint16_t y = raw_y;
  if (g_touch_swap_xy) {
    uint16_t t = x;
    x = y;
    y = t;
  }
  if (g_touch_invert_x) {
    x = (LCD_WIDTH > 0) ? (LCD_WIDTH - 1 - x) : x;
  }
  if (g_touch_invert_y) {
    y = (LCD_HEIGHT > 0) ? (LCD_HEIGHT - 1 - y) : y;
  }
  if (x >= LCD_WIDTH) {
    x = LCD_WIDTH - 1;
  }
  if (y >= LCD_HEIGHT) {
    y = LCD_HEIGHT - 1;
  }
  *disp_x = x;
  *disp_y = y;
}

static uint32_t g_touch_poll_ok = 0;
static uint32_t g_touch_poll_fail = 0;
static uint32_t g_touch_event_count = 0;

// jaapp burst read from TD_STATUS (0x02). Also returns raw 5 bytes for logging.
static bool poll_touch_raw(uint16_t *raw_x, uint16_t *raw_y, uint8_t *evt_out,
                           uint8_t *td_out, uint8_t raw5[5]) {
  uint8_t buf[5] = {0};

  Wire.beginTransmission(FT3168_I2C_ADDR);
  Wire.write(0x02);
  if (Wire.endTransmission(false) != 0) {
    g_touch_poll_fail++;
    return false;
  }
  if (Wire.requestFrom((int)FT3168_I2C_ADDR, 5) != 5) {
    g_touch_poll_fail++;
    return false;
  }
  for (int i = 0; i < 5; i++) {
    buf[i] = Wire.read();
    if (raw5) {
      raw5[i] = buf[i];
    }
  }
  g_touch_poll_ok++;

  const uint8_t td = buf[0] & 0x0F;
  const uint8_t evt = (buf[1] >> 6) & 0x03;
  *raw_x = ((uint16_t)(buf[1] & 0x0F) << 8) | buf[2];
  *raw_y = ((uint16_t)(buf[3] & 0x0F) << 8) | buf[4];
  if (td_out) {
    *td_out = td;
  }
  if (evt_out) {
    *evt_out = evt;
  }
  return td > 0;
}

static void update_touch_marker(uint16_t x, uint16_t y, bool pressed) {
  if (!g_display_ok || gfx == nullptr) {
    return;
  }

  if (g_touch_down) {
    // Local erase around previous marker (keep color bars intact above y=120)
    const int16_t erase_y0 = 120;
    gfx->fillRect(0, g_last_disp_y - 1, LCD_WIDTH, 3, RGB565_BLACK);
    gfx->fillRect(g_last_disp_x - 1, erase_y0, 3, LCD_HEIGHT - erase_y0 - 50, RGB565_BLACK);
    gfx->fillCircle(g_last_disp_x, g_last_disp_y, 10, RGB565_BLACK);
    gfx->drawRect(0, 0, LCD_WIDTH, LCD_HEIGHT, RGB565_WHITE);
    gfx->drawRect(1, 1, LCD_WIDTH - 2, LCD_HEIGHT - 2, RGB565_YELLOW);
  }

  if (!pressed) {
    g_touch_down = false;
    gfx->fillRect(8, 330, 264, 90, RGB565_BLACK);
    return;
  }

  gfx->drawFastHLine(0, y, LCD_WIDTH, RGB565_CYAN);
  gfx->drawFastVLine(x, 120, LCD_HEIGHT - 170, RGB565_CYAN);
  gfx->fillCircle(x, y, 8, RGB565_RED);
  gfx->drawCircle(x, y, 12, RGB565_YELLOW);

  gfx->fillRect(8, 330, 264, 90, RGB565_BLACK);
  gfx->setTextSize(3);
  gfx->setTextColor(RGB565_CYAN, RGB565_BLACK);
  gfx->setCursor(12, 336);
  gfx->printf("X=%u", x);
  gfx->setCursor(12, 368);
  gfx->printf("Y=%u", y);
  gfx->setTextSize(2);
  gfx->setTextColor(RGB565_YELLOW, RGB565_BLACK);
  gfx->setCursor(12, 404);
  gfx->printf("TAP #%lu", (unsigned long)g_touch_event_count);

  g_last_disp_x = x;
  g_last_disp_y = y;
  g_touch_down = true;
}

static const char *SD_TEST_PATH = "/esp2-selftest.txt";
static const char *SD_TEST_PAYLOAD =
    "ESP][ microSD self-test\n"
    "board=Waveshare ESP32-S3-Touch-AMOLED-1.64\n";
static const char *SD_PREFS_NS = "esp2sd";
static const char *SD_PREFS_KEY = "persist";

static void draw_sd_status(bool ok) {
  if (!g_display_ok || gfx == nullptr) {
    return;
  }
  gfx->fillRect(8, 420, 200, 28, RGB565_BLACK);
  gfx->setTextSize(2);
  if (ok) {
    gfx->setTextColor(RGB565_GREEN, RGB565_BLACK);
    gfx->setCursor(12, 424);
    gfx->print("SD OK");
  } else {
    gfx->setTextColor(RGB565_RED, RGB565_BLACK);
    gfx->setCursor(12, 424);
    gfx->print("SD FAIL");
  }
}

static const char *sd_card_type_name(uint8_t t) {
  switch (t) {
    case CARD_MMC:
      return "MMC";
    case CARD_SD:
      return "SDSC";
    case CARD_SDHC:
      return "SDHC";
    default:
      return "UNKNOWN";
  }
}

static void sd_list_root() {
  File root = SD.open("/");
  if (!root) {
    logf("SD", "[FAIL] cannot open root");
    return;
  }
  if (!root.isDirectory()) {
    logf("SD", "[FAIL] root is not a directory");
    root.close();
    return;
  }

  logf("SD", "root listing:");
  int count = 0;
  while (true) {
    File entry = root.openNextFile();
    if (!entry) {
      break;
    }
    logf("SD", "  %s %s", entry.isDirectory() ? "DIR " : "FILE", entry.name());
    entry.close();
    count++;
    if (count >= 64) {
      logf("SD", "  ... truncated");
      break;
    }
  }
  if (count == 0) {
    logf("SD", "  (empty)");
  }
  root.close();
}

static bool sd_read_file_all(const char *path, String *out) {
  File f = SD.open(path, FILE_READ);
  if (!f) {
    return false;
  }
  *out = f.readString();
  f.close();
  return true;
}

static bool sd_write_payload() {
  File f = SD.open(SD_TEST_PATH, FILE_WRITE);
  if (!f) {
    logf("SD", "[FAIL] open for write %s", SD_TEST_PATH);
    return false;
  }
  size_t n = f.print(SD_TEST_PAYLOAD);
  f.flush();
  f.close();
  if (n != strlen(SD_TEST_PAYLOAD)) {
    logf("SD", "[FAIL] short write %u/%u", (unsigned)n,
         (unsigned)strlen(SD_TEST_PAYLOAD));
    return false;
  }
  logf("SD", "write OK");
  return true;
}

static bool sd_verify_payload() {
  String got;
  if (!sd_read_file_all(SD_TEST_PATH, &got)) {
    logf("SD", "[FAIL] open for read %s", SD_TEST_PATH);
    return false;
  }
  logf("SD", "read-back OK (%u bytes)", (unsigned)got.length());
  if (got != SD_TEST_PAYLOAD) {
    logf("SD", "[FAIL] verify mismatch");
    logf("SD", "expected_len=%u got_len=%u",
         (unsigned)strlen(SD_TEST_PAYLOAD), (unsigned)got.length());
    return false;
  }
  logf("SD", "verify OK");
  return true;
}

static bool init_sd() {
  logf("SD", "initializing");
  logf("SD", "interface=SPI");
  logf("SD", "pins CS=%d MOSI=%d MISO=%d SCLK=%d",
       PIN_SD_CS, PIN_SD_MOSI, PIN_SD_MISO, PIN_SD_SCLK);

  // Dedicated SPI pins — do not share with QSPI display or touch I2C.
  SPI.begin(PIN_SD_SCLK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);

  if (!SD.begin(PIN_SD_CS, SPI, 4000000)) {
    logf("SD", "[FAIL] mount failed (begin)");
    g_sd_ok = false;
    return false;
  }

  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    logf("SD", "[FAIL] no card detected");
    g_sd_ok = false;
    return false;
  }

  logf("SD", "card detected");
  logf("SD", "type=%s", sd_card_type_name(cardType));
  logf("SD", "filesystem=FAT");

  uint64_t capacity = SD.cardSize();
  uint64_t total = SD.totalBytes();
  uint64_t used = SD.usedBytes();
  logf("SD", "capacity=%llu MB (%.2f GB)",
       (unsigned long long)(capacity / (1024ULL * 1024ULL)),
       (double)capacity / (1024.0 * 1024.0 * 1024.0));
  logf("SD", "free=%llu MB (total_fs=%llu MB used=%llu MB)",
       (unsigned long long)((total - used) / (1024ULL * 1024ULL)),
       (unsigned long long)(total / (1024ULL * 1024ULL)),
       (unsigned long long)(used / (1024ULL * 1024ULL)));

  sd_list_root();

  Preferences prefs;
  prefs.begin(SD_PREFS_NS, false);
  const bool persist_pending = prefs.getBool(SD_PREFS_KEY, false);

  if (persist_pending) {
    logf("SD", "persistence check after reboot");
    if (!SD.exists(SD_TEST_PATH)) {
      logf("SD", "[FAIL] persistence: %s missing after reboot", SD_TEST_PATH);
      prefs.putBool(SD_PREFS_KEY, false);
      prefs.end();
      g_sd_ok = false;
      g_sd_persist_ok = false;
      return false;
    }
    if (!sd_verify_payload()) {
      prefs.putBool(SD_PREFS_KEY, false);
      prefs.end();
      g_sd_ok = false;
      g_sd_persist_ok = false;
      return false;
    }
    prefs.putBool(SD_PREFS_KEY, false);
    prefs.end();
    logf("SD", "persistence OK");
    g_sd_ok = true;
    g_sd_persist_ok = true;
    return true;
  }

  // Already verified on a prior boot — remount check only (no reboot loop).
  if (SD.exists(SD_TEST_PATH) && sd_verify_payload()) {
    prefs.end();
    logf("SD", "prior self-test file OK (skip rewrite/reboot)");
    g_sd_ok = true;
    g_sd_persist_ok = true;
    return true;
  }

  // Fresh write + read-back, then reboot once for persistence.
  if (SD.exists(SD_TEST_PATH)) {
    SD.remove(SD_TEST_PATH);
  }
  if (!sd_write_payload()) {
    prefs.end();
    g_sd_ok = false;
    return false;
  }
  if (!sd_verify_payload()) {
    prefs.end();
    g_sd_ok = false;
    return false;
  }

  prefs.putBool(SD_PREFS_KEY, true);
  prefs.end();
  g_sd_ok = true;
  draw_sd_status(false); // not fully OK until persistence
  if (g_display_ok && gfx != nullptr) {
    gfx->fillRect(8, 420, 200, 28, RGB565_BLACK);
    gfx->setTextSize(2);
    gfx->setTextColor(RGB565_YELLOW, RGB565_BLACK);
    gfx->setCursor(12, 424);
    gfx->print("SD REBOOT");
  }
  logf("SD", "reboot for persistence test...");
  delay(500);
  Serial.flush();
  ESP.restart();
  return true; // unreachable
}

static void draw_imu_status(bool ok) {
  if (!g_display_ok || gfx == nullptr) {
    return;
  }
  gfx->fillRect(140, 420, 130, 28, RGB565_BLACK);
  gfx->setTextSize(2);
  if (ok) {
    gfx->setTextColor(RGB565_GREEN, RGB565_BLACK);
    gfx->setCursor(148, 424);
    gfx->print("IMU OK");
  } else {
    gfx->setTextColor(RGB565_RED, RGB565_BLACK);
    gfx->setCursor(148, 424);
    gfx->print("IMU FAIL");
  }
}

// BLE scan UI: SCAN AGAIN button region (bottom)
static constexpr int16_t BLE_BTN_X = 20;
static constexpr int16_t BLE_BTN_Y = 400;
static constexpr int16_t BLE_BTN_W = 240;
static constexpr int16_t BLE_BTN_H = 44;

static bool ble_hit_scan_again(uint16_t x, uint16_t y) {
  return x >= (uint16_t)BLE_BTN_X && x < (uint16_t)(BLE_BTN_X + BLE_BTN_W) &&
         y >= (uint16_t)BLE_BTN_Y && y < (uint16_t)(BLE_BTN_Y + BLE_BTN_H);
}

static void draw_ble_scan_screen(const BleScanReport *report, const char *status) {
  if (!g_display_ok || gfx == nullptr) {
    return;
  }

  gfx->fillScreen(RGB565_BLACK);
  gfx->setTextSize(2);
  gfx->setTextColor(RGB565_CYAN, RGB565_BLACK);
  gfx->setCursor(16, 16);
  gfx->print("BLE SCAN");

  gfx->setTextSize(1);
  gfx->setTextColor(RGB565_YELLOW, RGB565_BLACK);
  gfx->setCursor(16, 48);
  gfx->print(status ? status : "");

  if (status != nullptr) {
    strncpy(g_ble_status_line, status, sizeof(g_ble_status_line) - 1);
    g_ble_status_line[sizeof(g_ble_status_line) - 1] = '\0';
  }

  gfx->setTextColor(RGB565_WHITE, RGB565_BLACK);
  if (report == nullptr || report->count <= 0) {
    gfx->setCursor(16, 80);
    gfx->print("(no devices yet)");
    gfx->setCursor(16, 100);
    gfx->print("Put Nimbus in pairing");
  } else {
    int order[BLE_SCAN_MAX_HITS];
    const int n = report->count;
    for (int i = 0; i < n; i++) {
      order[i] = i;
    }
    for (int i = 1; i < n; i++) {
      int key = order[i];
      int j = i - 1;
      while (j >= 0 && report->hits[order[j]].rssi < report->hits[key].rssi) {
        order[j + 1] = order[j];
        j--;
      }
      order[j + 1] = key;
    }

    int16_t y = 72;
    const int max_rows = 12;
    for (int r = 0; r < n && r < max_rows; r++) {
      const BleScanHit &h = report->hits[order[r]];
      gfx->setTextSize(2);
      gfx->setTextColor(RGB565_WHITE, RGB565_BLACK);
      gfx->setCursor(12, y);
      char line[28];
      snprintf(line, sizeof(line), "%.18s", h.name);
      gfx->print(line);
      y += 22;
      gfx->setTextSize(1);
      gfx->setTextColor(RGB565_GREEN, RGB565_BLACK);
      gfx->setCursor(20, y);
      gfx->printf("%d dBm", (int)h.rssi);
      y += 16;
      if (y > 370) {
        break;
      }
    }
  }

  gfx->fillRect(BLE_BTN_X, BLE_BTN_Y, BLE_BTN_W, BLE_BTN_H, RGB565_BLUE);
  gfx->drawRect(BLE_BTN_X, BLE_BTN_Y, BLE_BTN_W, BLE_BTN_H, RGB565_WHITE);
  gfx->setTextSize(2);
  gfx->setTextColor(RGB565_WHITE, RGB565_BLUE);
  gfx->setCursor(BLE_BTN_X + 36, BLE_BTN_Y + 14);
  gfx->print("SCAN AGAIN");

  g_ble_screen_active = true;
}

static void restore_ui_after_wake() {
  if (!g_display_ok || gfx == nullptr) {
    return;
  }
  if (g_ble_screen_active) {
    draw_ble_scan_screen(&g_ble_report, g_ble_status_line);
  } else {
    draw_diagnostic_screen();
    draw_sd_status(g_sd_ok && g_sd_persist_ok);
    draw_imu_status(g_imu_ok);
  }
}

static void draw_vr_diag_screen(const char *status_line) {
  if (!g_display_ok || gfx == nullptr) {
    return;
  }
  gfx->fillScreen(RGB565_BLACK);
  gfx->setTextSize(2);
  gfx->setTextColor(RGB565_CYAN, RGB565_BLACK);
  gfx->setCursor(12, 12);
  gfx->print("VR PARK CONN");

  gfx->setTextSize(1);
  gfx->setTextColor(RGB565_YELLOW, RGB565_BLACK);
  gfx->setCursor(12, 40);
  gfx->print(status_line ? status_line : "");

  gfx->setTextColor(RGB565_WHITE, RGB565_BLACK);
  int16_t y = 60;
  auto line = [&](const char *t) {
    gfx->setCursor(12, y);
    gfx->print(t);
    y += 14;
  };

  if (g_vr_diag_done) {
    char buf[56];
    snprintf(buf, sizeof(buf), "addr %s", g_vr_diag.address);
    line(buf);
    snprintf(buf, sizeof(buf), "type %u connable=%s", (unsigned)g_vr_diag.addr_type,
             g_vr_diag.connectable ? "Y" : "N");
    line(buf);
    snprintf(buf, sizeof(buf), "flags=0x%02X advType=%u", (unsigned)g_vr_diag.adv_flags,
             (unsigned)g_vr_diag.adv_type);
    line(buf);
    line(g_vr_diag.connect_ok ? "connect: YES" : "connect: FAIL");
    if (!g_vr_diag.connect_ok) {
      snprintf(buf, sizeof(buf), "rc=%d", g_vr_diag.connect_rc);
      line(buf);
      line(g_vr_diag.rc_text);
      if (g_vr_diag.hci_status >= 0) {
        line(g_vr_diag.hci_text);
      }
    } else {
      snprintf(buf, sizeof(buf), "MTU=%u sec=%s", (unsigned)g_vr_diag.mtu,
               g_vr_diag.secured ? "OK" : "FAIL");
      line(buf);
    }
    line(g_vr_diag.summary);
    gfx->setTextColor(RGB565_GREEN, RGB565_BLACK);
    line("See [VRPARK] serial");
  }

  gfx->fillRect(BLE_BTN_X, BLE_BTN_Y, BLE_BTN_W, BLE_BTN_H, RGB565_BLUE);
  gfx->drawRect(BLE_BTN_X, BLE_BTN_Y, BLE_BTN_W, BLE_BTN_H, RGB565_WHITE);
  gfx->setTextSize(2);
  gfx->setTextColor(RGB565_WHITE, RGB565_BLUE);
  gfx->setCursor(BLE_BTN_X + 28, BLE_BTN_Y + 14);
  gfx->print("RE-DIAG");
  g_ble_screen_active = true;
}

static void run_vr_park_conn_diag() {
  if (g_ble_scan_busy) {
    return;
  }
  g_ble_scan_busy = true;
  draw_vr_diag_screen("Diagnosing...");
  Serial.flush();

  VrParkConnDiag result;
  const bool ok = ble_vrpark_conn_diag_run(&result);
  g_vr_diag = result;
  g_vr_diag_done = true;

  draw_vr_diag_screen(result.summary[0] ? result.summary
                                        : (ok ? "OK" : "CONN FAIL"));
  logf("REGRESSION", "DISPLAY=%s TOUCH=%s SD=%s IMU=%s BLE-SCAN=%s",
       g_display_ok ? "OK" : "FAIL", g_touch_ok ? "OK" : "FAIL",
       (g_sd_ok && g_sd_persist_ok) ? "OK" : "FAIL", g_imu_ok ? "OK" : "FAIL",
       g_ble_ok ? "OK" : "FAIL");
  g_ble_scan_busy = false;
}

static void run_ble_scan_and_show() {
  if (g_ble_scan_busy) {
    return;
  }
  g_ble_scan_busy = true;
  draw_ble_scan_screen(&g_ble_report, "Scanning 15s...");
  Serial.flush();

  BleScanReport report;
  const bool ok = ble_scan_diag_run(&report, BLE_SCAN_DURATION_SEC);
  if (ok) {
    g_ble_report = report;
    char status[48];
    snprintf(status, sizeof(status), "hits=%d  %lums", report.count,
             (unsigned long)report.duration_ms);
    draw_ble_scan_screen(&g_ble_report, status);
  } else {
    draw_ble_scan_screen(&g_ble_report, "SCAN FAIL");
    Serial.println("[BLE-SCAN][FAIL] scan run");
  }
  g_ble_scan_busy = false;

  // Prefer VR PARK connection diagnosis when present — one shot only.
  bool saw_vr = false;
  for (int i = 0; i < g_ble_report.count; i++) {
    if (strstr(g_ble_report.hits[i].name, "VR PARK") ||
        strstr(g_ble_report.hits[i].name, "VR Park") ||
        strstr(g_ble_report.hits[i].name, "vr park")) {
      saw_vr = true;
      break;
    }
  }
  if (saw_vr && !g_vr_diag_done) {
    g_vr_auto_diag_done = true;
    logf("BLE", "VR PARK seen — one-shot connection diagnosis");
    run_vr_park_conn_diag();
  }
}
static bool init_imu() {
  logf("IMU", "starting");
  logf("IMU", "shared I2C SDA=%d SCL=%d (with FT3168)", PIN_TOUCH_SDA,
       PIN_TOUCH_SCL);
  pinMode(PIN_IMU_INT1, INPUT); // unused for polling bring-up
  logf("IMU", "INT1=GPIO%d (input, polling mode)", PIN_IMU_INT1);

  // Do NOT Wire.begin() again — reuse existing bus after touch init.
  Wire.setClock(300000);

  if (!g_imu.begin(Wire)) {
    logf("IMU", "[FAIL] QMI8658 not detected / WHO_AM_I mismatch");
    return false;
  }

  logf("IMU", "QMI8658 detected");
  logf("IMU", "address=0x%02X", g_imu.address());
  logf("IMU", "device-id=0x%02X revision=0x%02X", g_imu.whoAmI(),
       g_imu.revision());

  Qmi8658RawSample raw;
  bool got = false;
  for (int i = 0; i < 30; i++) {
    if (g_imu.readRaw(&raw)) {
      got = true;
      break;
    }
    delay(20);
  }
  if (!got) {
    logf("IMU", "[FAIL] no raw sample");
    return false;
  }
  logf("IMU", "accelerometer OK");
  logf("IMU", "gyroscope OK");
  logf("IMU", "units accel=g gyro=dps range_acc=+/-8g range_gyr=+/-1024dps");
  Serial.printf("[IMU] #%lu raw acc=%d,%d,%d gyro=%d,%d,%d st=0x%02X ctrl7=0x%02X\n",
                (unsigned long)raw.n, (int)raw.ax, (int)raw.ay, (int)raw.az,
                (int)raw.gx, (int)raw.gy, (int)raw.gz, raw.status0, raw.ctrl7);
  logf("IMU", "ready");
  return true;
}

static void report_selftest() {
  const bool software_pass = g_flash_ok && g_psram_ok && g_display_ok &&
                             g_touch_ok && g_sd_ok && g_sd_persist_ok &&
                             g_imu_ok;
  if (software_pass) {
    logf("SELFTEST", "SOFTWARE PASS");
  } else {
    logf("SELFTEST",
         "SOFTWARE FAIL flash=%d psram=%d display=%d touch=%d sd=%d persist=%d "
         "imu=%d",
         g_flash_ok, g_psram_ok, g_display_ok, g_touch_ok, g_sd_ok,
         g_sd_persist_ok, g_imu_ok);
  }
  logf("REGRESSION", "DISPLAY=%s TOUCH=%s SD=%s IMU=%s BLE-SCAN=%s",
       g_display_ok ? "OK" : "FAIL", g_touch_ok ? "OK" : "FAIL",
       (g_sd_ok && g_sd_persist_ok) ? "OK" : "FAIL", g_imu_ok ? "OK" : "FAIL",
       g_ble_ok ? "OK" : "FAIL");
  if (g_touch_ok) {
    logf("VISUAL", "TOUCH_ACTIVE");
  }
  if (g_sd_ok && g_sd_persist_ok) {
    logf("VISUAL", "SD_OK");
  }
  if (g_imu_ok) {
    logf("VISUAL", "IMU_OK_TILT_BOARD");
  } else {
    logf("VISUAL", "IMU_FAILED");
  }
  if (g_ble_ok) {
    logf("VISUAL", "BLE_SCAN_READY_PAIR_NIMBUS");
  }
}

void setup() {
  Serial.begin(115200);
  uint32_t wait_start = millis();
  while (!Serial && (millis() - wait_start) < 8000) {
    delay(10);
  }
  delay(200);

  report_chip();
  report_memory();

  // Release shared LCD/TP reset before bus/panel bring-up.
  touch_prepare_gpio();
  pulse_shared_tp_lcd_reset(20, 200);

  Wire.begin(PIN_TOUCH_SDA, PIN_TOUCH_SCL);
  Wire.setClock(100000);
  delay(20);
  i2c_scan_bus("pre-display");

  g_display_ok = init_display();
  if (g_display_ok) {
    draw_diagnostic_screen();
  }

  // Keep same Wire instance; only ensure clock after QSPI traffic.
  Wire.setClock(100000);
  delay(20);
  i2c_scan_bus("post-display-immediate");

  g_touch_ok = init_touch_after_display();

  if (g_display_ok && g_touch_ok) {
    draw_diagnostic_screen();
    gfx->setTextSize(2);
    gfx->setTextColor(RGB565_GREEN, RGB565_BLACK);
    gfx->setCursor(20, 310);
    gfx->print("TOUCH READY");
  } else if (g_display_ok) {
    gfx->setTextSize(2);
    gfx->setTextColor(RGB565_RED, RGB565_BLACK);
    gfx->setCursor(20, 310);
    gfx->print("TOUCH FAIL");
  }

  // SD self-test may ESP.restart() once for persistence; second boot finishes.
  const bool sd_pass = init_sd();
  draw_sd_status(sd_pass && g_sd_persist_ok);

  // If we are about to restart, skip final selftest (unreachable after restart()).
  if (!g_sd_persist_ok && g_sd_ok) {
    // First-pass write OK — restart is pending inside init_sd.
    return;
  }

  g_imu_ok = init_imu();
  draw_imu_status(g_imu_ok);

  // Confirm touch still ACKs after IMU register traffic on shared bus.
  if (g_touch_ok && !i2c_probe_addr(FT3168_I2C_ADDR)) {
    logf("TOUCH", "[FAIL] FT3168 lost after IMU init");
    g_touch_ok = false;
  } else if (g_touch_ok) {
    logf("TOUCH", "still OK after IMU init");
  }

  logf("RAM", "before BLE heap_free=%u psram_free=%u", ESP.getFreeHeap(),
       ESP.getFreePsram());
  g_ble_ok = ble_scan_diag_init();
  logf("RAM", "after BLE heap_free=%u psram_free=%u", ESP.getFreeHeap(),
       ESP.getFreePsram());

  // Touch still alive after BLE stack init?
  if (g_touch_ok && !i2c_probe_addr(FT3168_I2C_ADDR)) {
    logf("TOUCH", "[FAIL] FT3168 lost after BLE init");
    g_touch_ok = false;
  }

  report_memory();
  report_selftest();

  if (g_ble_ok) {
    run_ble_scan_and_show();
  } else if (g_display_ok) {
    gfx->setTextSize(2);
    gfx->setTextColor(RGB565_RED, RGB565_BLACK);
    gfx->setCursor(16, 340);
    gfx->print("BLE FAIL");
  }

  if (g_display_ok) {
    display_power_begin_after_ui();
  }
}

void loop() {
  static uint32_t last_imu_read_ms = 0;
  static uint32_t last_imu_log_ms = 0;
  static uint32_t last_heartbeat_ms = 0;
  static bool was_pressed = false;
  static uint16_t last_logged_x = 0xFFFF;
  static uint16_t last_logged_y = 0xFFFF;
  static uint32_t last_move_log_ms = 0;
  static int last_int = 1;
  static bool deferred_vr_diag_once = false;
  static uint32_t loop_start_ms = 0;
  if (loop_start_ms == 0) {
    loop_start_ms = millis();
  }

  // One deferred diagnosis if boot scan didn't already (USB-CDC catch). No retries.
  if (g_ble_ok && !g_ble_scan_busy && !g_vr_diag_done && !g_vr_auto_diag_done &&
      !deferred_vr_diag_once && (millis() - loop_start_ms) >= 4000) {
    deferred_vr_diag_once = true;
    g_vr_auto_diag_done = true;
    logf("BLE", "one-shot deferred VR PARK connection diagnosis");
    run_vr_park_conn_diag();
  }

  if (!g_touch_ok) {
    static uint32_t last_retry_ms = 0;
    const uint32_t now = millis();
    if ((now - last_retry_ms) >= 2000) {
      last_retry_ms = now;
      if (try_recover_touch()) {
        g_touch_ok = true;
        if (g_display_ok) {
          draw_diagnostic_screen();
          gfx->setTextSize(2);
          gfx->setTextColor(RGB565_GREEN, RGB565_BLACK);
          gfx->setCursor(20, 310);
          gfx->print("TOUCH READY");
          draw_sd_status(g_sd_ok && g_sd_persist_ok);
          draw_imu_status(g_imu_ok);
        }
      } else {
        logf("TOUCH", "retry: 0x38 still absent");
      }
    }
  } else {
    uint16_t raw_x = 0;
    uint16_t raw_y = 0;
    uint8_t evt = 0xFF;
    uint8_t td = 0;
    uint8_t raw5[5] = {0};
    bool pressed = poll_touch_raw(&raw_x, &raw_y, &evt, &td, raw5);

    const int int_level = digitalRead(PIN_TP_INT);
    if (int_level == 0 && last_int != 0 && !pressed) {
      Serial.printf("[TOUCH] INT low raw=%02X %02X %02X %02X %02X\n",
                    raw5[0], raw5[1], raw5[2], raw5[3], raw5[4]);
      Serial.flush();
    }
    last_int = int_level;

    if (pressed) {
      uint16_t dx = 0;
      uint16_t dy = 0;
      raw_to_display(raw_x, raw_y, &dx, &dy);

      const uint32_t now = millis();
      const bool moved = (dx != last_logged_x) || (dy != last_logged_y);
      const bool move_due = (now - last_move_log_ms) >= 80;

      if (!was_pressed) {
        g_touch_event_count++;
        Serial.printf(
            "[TOUCH] DOWN x=%u y=%u raw=%u,%u td=%u evt=%u int=%d "
            "hex=%02X%02X%02X%02X%02X n=%lu\n",
            dx, dy, raw_x, raw_y, td, evt, int_level, raw5[0], raw5[1],
            raw5[2], raw5[3], raw5[4], (unsigned long)g_touch_event_count);
        Serial.flush();
        last_move_log_ms = now;
        last_logged_x = dx;
        last_logged_y = dy;

        // Wake consumes this touch so it cannot activate UI underneath.
        const bool wake_consumed = g_display_power.notifyActivity("touch");
        if (!wake_consumed && g_display_power.isInteractive() &&
            g_ble_screen_active && !g_ble_scan_busy &&
            ble_hit_scan_again(dx, dy)) {
          if (g_vr_diag_done) {
            logf("BLE", "RE-DIAG touched (manual)");
            run_vr_park_conn_diag();
          } else {
            logf("BLE-SCAN", "SCAN AGAIN touched");
            run_ble_scan_and_show();
          }
        }
      } else if (moved && move_due) {
        Serial.printf("[TOUCH] x=%u y=%u raw=%u,%u td=%u evt=%u int=%d\n", dx,
                      dy, raw_x, raw_y, td, evt, int_level);
        Serial.flush();
        last_move_log_ms = now;
        last_logged_x = dx;
        last_logged_y = dy;
        if (g_display_power.isInteractive()) {
          (void)g_display_power.notifyActivity("touch");
        }
      }

      if (g_display_power.isInteractive() && !g_ble_screen_active) {
        update_touch_marker(dx, dy, true);
      }
      was_pressed = true;
    } else if (was_pressed) {
      Serial.printf("[TOUCH] UP last=%u,%u\n", last_logged_x, last_logged_y);
      Serial.flush();
      if (g_display_power.isInteractive() && !g_ble_screen_active) {
        update_touch_marker(0, 0, false);
      }
      was_pressed = false;
      last_logged_x = 0xFFFF;
      last_logged_y = 0xFFFF;
    }
  }

  const uint32_t now = millis();
  g_display_power.update(now);

  // Keep IMU acquisition at ~10 Hz; throttle only the serial diagnostic line.
  if (g_imu_ok && (now - last_imu_read_ms) >= 100) {
    last_imu_read_ms = now;
    Qmi8658RawSample raw;
    if (g_imu.readRaw(&raw)) {
      if ((now - last_imu_log_ms) >= 1000) {
        last_imu_log_ms = now;
        Serial.printf(
            "[IMU] #%lu raw acc=%d,%d,%d gyro=%d,%d,%d st=0x%02X ctrl7=0x%02X\n",
            (unsigned long)raw.n, (int)raw.ax, (int)raw.ay, (int)raw.az,
            (int)raw.gx, (int)raw.gy, (int)raw.gz, raw.status0, raw.ctrl7);
      }
    } else {
      Serial.println("[IMU][FAIL] raw read I2C");
    }
  }

  if (g_touch_ok && (now - last_heartbeat_ms) >= 10000) {
    last_heartbeat_ms = now;
    ft3168_write_u8(0xA4, 0x00);
    ft3168_write_u8(0xA5, 0x00);
    logf("TOUCHHB", "poll ok=%lu fail=%lu events=%lu int=%d",
         (unsigned long)g_touch_poll_ok, (unsigned long)g_touch_poll_fail,
         (unsigned long)g_touch_event_count, digitalRead(PIN_TP_INT));
    if (g_vr_diag_done) {
      logf("BLE", "VR diag connect_ok=%d rc=%d hci=0x%X", (int)g_vr_diag.connect_ok,
           g_vr_diag.connect_rc, g_vr_diag.hci_status);
    }
  }

  delay(20);
}
