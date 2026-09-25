#pragma once

#include <Arduino.h>
#include <Wire.h>

// Minimal QMI8658 bring-up for Waveshare ESP32-S3-Touch-AMOLED-1.64.
// Evidence: Waveshare Arduino example 02_I2C_QMI8658 (SDA=47 SCL=48,
// WHO_AM_I=0x05, slave 0x6A/0x6B). Shares Wire bus with FT3168 — never
// calls Wire.begin(); caller must already own the bus.

struct Qmi8658Sample {
  float ax_g;
  float ay_g;
  float az_g;
  float gx_dps;
  float gy_dps;
  float gz_dps;
};

struct Qmi8658RawSample {
  uint32_t n;
  uint8_t status0;
  uint8_t ctrl7;
  int16_t ax;
  int16_t ay;
  int16_t az;
  int16_t gx;
  int16_t gy;
  int16_t gz;
  uint8_t raw12[12];
};

class Qmi8658Min {
 public:
  bool begin(TwoWire &wire = Wire);

  // Always performs a fresh I2C read of output registers (no caching).
  // Returns false only on I2C failure.
  bool readRaw(Qmi8658RawSample *out);

  bool read(Qmi8658Sample *out);

  void dumpCtrlRegs();

  uint8_t address() const { return addr_; }
  uint8_t whoAmI() const { return who_; }
  uint8_t revision() const { return rev_; }
  uint32_t sampleCount() const { return sample_n_; }

 private:
  bool probeAddress(uint8_t addr);
  bool writeReg(uint8_t reg, uint8_t val);
  bool readRegs(uint8_t reg, uint8_t *buf, size_t len);
  bool readRegsOneByOne(uint8_t start_reg, uint8_t *buf, size_t len);

  TwoWire *wire_ = nullptr;
  uint8_t addr_ = 0;
  uint8_t who_ = 0;
  uint8_t rev_ = 0;
  int ssvt_a_ = 4096;  // Acc ±8g
  int ssvt_g_ = 32;    // Gyro ±1024 dps
  uint32_t sample_n_ = 0;
  bool use_byte_reads_ = false;
};

extern Qmi8658Min g_imu;
