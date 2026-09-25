#include "qmi8658_min.h"

// Register map / constants from Waveshare qmi8658c.h + QMI8658 datasheet
static const uint8_t REG_WHO_AM_I = 0x00;
static const uint8_t REG_REVISION = 0x01;
static const uint8_t REG_CTRL1 = 0x02;
static const uint8_t REG_CTRL2 = 0x03;
static const uint8_t REG_CTRL3 = 0x04;
static const uint8_t REG_CTRL5 = 0x06;
static const uint8_t REG_CTRL7 = 0x08;
static const uint8_t REG_CTRL8 = 0x09;
static const uint8_t REG_STATUS0 = 0x2E;
static const uint8_t REG_AX_L = 0x35;
static const uint8_t REG_RESET = 0x60;

static const uint8_t WHO_AM_I_VALUE = 0x05;
static const uint8_t ADDR_L = 0x6A;
static const uint8_t ADDR_H = 0x6B;

// Waveshare qmi8658_config_reg(0): Acc ±8g @ 250Hz, Gyro ±1024dps @ 250Hz
static const uint8_t CTRL2_ACC = (0x02 << 4) | 0x05;  // 8g | 250Hz
static const uint8_t CTRL3_GYR = (0x06 << 4) | 0x05;  // 1024dps | 250Hz
static const uint8_t CTRL7_ACCGYR = 0x03;

// CTRL1: ADDR_AI (bit6) | little-endian (BE=0). Datasheet default has BE=1;
// Waveshare uses 0x60 (AI|BE). We use 0x40 so host LE packing matches L then H.
static const uint8_t CTRL1_AI_LE = 0x40;

Qmi8658Min g_imu;

bool Qmi8658Min::writeReg(uint8_t reg, uint8_t val) {
  wire_->beginTransmission(addr_);
  wire_->write(reg);
  wire_->write(val);
  return wire_->endTransmission() == 0;
}

bool Qmi8658Min::readRegs(uint8_t reg, uint8_t *buf, size_t len) {
  wire_->beginTransmission(addr_);
  wire_->write(reg);
  if (wire_->endTransmission(false) != 0) {
    return false;
  }
  if (wire_->requestFrom((int)addr_, (int)len) != (int)len) {
    return false;
  }
  for (size_t i = 0; i < len; i++) {
    buf[i] = wire_->read();
  }
  return true;
}

bool Qmi8658Min::readRegsOneByOne(uint8_t start_reg, uint8_t *buf, size_t len) {
  for (size_t i = 0; i < len; i++) {
    if (!readRegs((uint8_t)(start_reg + i), &buf[i], 1)) {
      return false;
    }
  }
  return true;
}

bool Qmi8658Min::probeAddress(uint8_t addr) {
  addr_ = addr;
  uint8_t id = 0;
  if (!readRegs(REG_WHO_AM_I, &id, 1)) {
    return false;
  }
  who_ = id;
  return id == WHO_AM_I_VALUE;
}

void Qmi8658Min::dumpCtrlRegs() {
  uint8_t regs[8] = {0};
  if (!readRegs(REG_CTRL1, regs, 8)) {
    Serial.println("[IMU][FAIL] ctrl dump read");
    return;
  }
  Serial.printf(
      "[IMU] CTRL1..8 = %02X %02X %02X %02X %02X %02X %02X %02X\n", regs[0],
      regs[1], regs[2], regs[3], regs[4], regs[5], regs[6], regs[7]);
}

bool Qmi8658Min::begin(TwoWire &wire) {
  wire_ = &wire;
  addr_ = 0;
  who_ = 0;
  rev_ = 0;
  sample_n_ = 0;
  use_byte_reads_ = false;

  // Prefer 0x6B (this board / Waveshare hardcoded), then 0x6A.
  const uint8_t candidates[2] = {ADDR_H, ADDR_L};
  bool found = false;
  for (uint8_t a : candidates) {
    for (int retry = 0; retry < 5; retry++) {
      if (probeAddress(a)) {
        found = true;
        break;
      }
      delay(5);
    }
    if (found) {
      break;
    }
  }
  if (!found) {
    return false;
  }

  // Soft reset — Linux/Waveshare use Reset=0xB0; allow full settle (not 20ms).
  if (!writeReg(REG_RESET, 0xB0)) {
    return false;
  }
  delay(100);

  uint8_t id = 0;
  if (!readRegs(REG_WHO_AM_I, &id, 1) || id != WHO_AM_I_VALUE) {
    return false;
  }
  who_ = id;
  readRegs(REG_REVISION, &rev_, 1);

  // Disable sensors while configuring (Waveshare config_reg sequence).
  if (!writeReg(REG_CTRL7, 0x00)) {
    return false;
  }
  // ADDR auto-increment, little-endian; SensorDisable=0 (oscillator on).
  if (!writeReg(REG_CTRL1, CTRL1_AI_LE)) {
    return false;
  }
  // No motion-detect / datavalid routing for simple polling.
  if (!writeReg(REG_CTRL8, 0x00)) {
    return false;
  }

  ssvt_a_ = (1 << 12);  // ±8g
  if (!writeReg(REG_CTRL2, CTRL2_ACC)) {
    return false;
  }
  ssvt_g_ = 32;  // ±1024 dps
  if (!writeReg(REG_CTRL3, CTRL3_GYR)) {
    return false;
  }
  if (!writeReg(REG_CTRL5, 0x00)) {
    return false;
  }

  // Enable accel + gyro. Gyro wake ~150ms (RIOT/QST guidance).
  if (!writeReg(REG_CTRL7, CTRL7_ACCGYR)) {
    return false;
  }
  delay(200);

  dumpCtrlRegs();

  // Verify Ctrl7 stuck enabled and Ctrl1 still AI / not SensorDisable.
  uint8_t ctrl1 = 0;
  uint8_t ctrl7 = 0;
  if (!readRegs(REG_CTRL1, &ctrl1, 1) || !readRegs(REG_CTRL7, &ctrl7, 1)) {
    return false;
  }
  if ((ctrl1 & 0x01) != 0) {
    Serial.println("[IMU][FAIL] SensorDisable bit set in CTRL1");
    return false;
  }
  if ((ctrl7 & 0x03) != 0x03) {
    Serial.printf("[IMU][FAIL] CTRL7=0x%02X (expected accel+gyro enable)\n",
                  ctrl7);
    return false;
  }

  // Prove burst auto-increment vs byte reads once.
  uint8_t burst[12] = {0};
  uint8_t single[12] = {0};
  if (readRegs(REG_AX_L, burst, 12) && readRegsOneByOne(REG_AX_L, single, 12)) {
    bool match = true;
    for (int i = 0; i < 12; i++) {
      if (burst[i] != single[i]) {
        match = false;
        break;
      }
    }
    Serial.printf("[IMU] burst_vs_byte %s\n", match ? "MATCH" : "MISMATCH");
    if (!match) {
      use_byte_reads_ = true;
      Serial.println("[IMU] falling back to per-byte register reads");
    }
  }

  return true;
}

bool Qmi8658Min::readRaw(Qmi8658RawSample *out) {
  if (wire_ == nullptr || addr_ == 0 || out == nullptr) {
    return false;
  }

  uint8_t status = 0;
  uint8_t ctrl7 = 0;
  if (!readRegs(REG_STATUS0, &status, 1)) {
    return false;
  }
  if (!readRegs(REG_CTRL7, &ctrl7, 1)) {
    return false;
  }

  uint8_t buf[12] = {0};
  const bool ok = use_byte_reads_ ? readRegsOneByOne(REG_AX_L, buf, 12)
                                  : readRegs(REG_AX_L, buf, 12);
  if (!ok) {
    return false;
  }

  sample_n_++;
  out->n = sample_n_;
  out->status0 = status;
  out->ctrl7 = ctrl7;
  for (int i = 0; i < 12; i++) {
    out->raw12[i] = buf[i];
  }

  // Little-endian packing of Ax_L/Ax_H ... (CTRL1 BE=0).
  out->ax = (int16_t)((uint16_t)(buf[1] << 8) | buf[0]);
  out->ay = (int16_t)((uint16_t)(buf[3] << 8) | buf[2]);
  out->az = (int16_t)((uint16_t)(buf[5] << 8) | buf[4]);
  out->gx = (int16_t)((uint16_t)(buf[7] << 8) | buf[6]);
  out->gy = (int16_t)((uint16_t)(buf[9] << 8) | buf[8]);
  out->gz = (int16_t)((uint16_t)(buf[11] << 8) | buf[10]);
  return true;
}

bool Qmi8658Min::read(Qmi8658Sample *out) {
  Qmi8658RawSample raw;
  if (!readRaw(&raw) || out == nullptr) {
    return false;
  }
  out->ax_g = (float)raw.ax / (float)ssvt_a_;
  out->ay_g = (float)raw.ay / (float)ssvt_a_;
  out->az_g = (float)raw.az / (float)ssvt_a_;
  out->gx_dps = (float)raw.gx / (float)ssvt_g_;
  out->gy_dps = (float)raw.gy / (float)ssvt_g_;
  out->gz_dps = (float)raw.gz / (float)ssvt_g_;
  return true;
}
