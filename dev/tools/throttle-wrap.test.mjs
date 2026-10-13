/**
 * Host/unit regression for Apple II throttle wall-clock math.
 * Simulates the old uint32 micros() wrap vs int64 esp_timer_get_time().
 * Does not flash hardware or wait 71 real minutes.
 */
import test from "node:test";
import assert from "node:assert/strict";

const kAppleIiHz = 1023000n;
const UINT32 = 0x100000000n;

/**
 * Legacy throttle sleep (bug): wall elapsed as uint32 modular delta.
 * @param {bigint} emuSinceCycles
 * @param {bigint} wallStartUs absolute µs at resync
 * @param {bigint} nowUs absolute µs now (monotonic)
 */
function sleepUsUint32Micros(emuSinceCycles, wallStartUs, nowUs) {
  const targetUs = (emuSinceCycles * 1000000n) / kAppleIiHz;
  const wallUs = (nowUs - wallStartUs) % UINT32; // models uint32 micros()-start
  if (targetUs > wallUs + 50n) return targetUs - wallUs;
  return 0n;
}

/**
 * Fixed throttle sleep: int64 monotonic elapsed.
 */
function sleepUsInt64EspTimer(emuSinceCycles, wallStartUs, nowUs) {
  const targetUs = (emuSinceCycles * 1000000n) / kAppleIiHz;
  const wallUs = nowUs - wallStartUs;
  if (targetUs > wallUs + 50n) return targetUs - wallUs;
  return 0n;
}

test("legacy uint32 micros wrap near 71.6 min produces absurd sleep", () => {
  // After ~89 min of Apple II time (matches observed frozen cycles≈5.47e9).
  const emuSince = 5468526249n;
  const wallStartUs = 0n;
  // Absolute time past one uint32 wrap (~71.6 min) + 1 s.
  const nowUs = UINT32 + 1_000_000n;
  const sleep = sleepUsUint32Micros(emuSince, wallStartUs, nowUs);
  // targetUs ≈ 5.35e9; wallUs32 ≈ 1e6 → sleep ≈ 5.3e9 µs (~89 min).
  assert.ok(sleep > 60n * 1_000_000n, `expected multi-minute sleep, got ${sleep}`);
});

test("int64 esp_timer path stays near realtime across old wrap boundary", () => {
  const wallStartUs = 0n;
  // Just before and after the old uint32 boundary.
  for (const nowUs of [UINT32 - 1_000_000n, UINT32, UINT32 + 1_000_000n, 90n * 60n * 1_000_000n]) {
    // Emulation kept roughly realtime since start.
    const emuSince = (nowUs * kAppleIiHz) / 1_000_000n;
    const sleep = sleepUsInt64EspTimer(emuSince, wallStartUs, nowUs);
    // Guard threshold in firmware is 2_000_000 µs; normal pacing stays far below.
    assert.ok(
      sleep < 2_000_000n,
      `at now=${nowUs} sleep=${sleep} must stay under sleep-guard (2s)`,
    );
  }
});

test("sleep guard would catch corrupt bases without busy-looping", () => {
  // Emulation far ahead of wall (corrupt bases after wrap-style bug).
  const sleep = sleepUsInt64EspTimer(10_000_000_000n, 0n, 1_000_000n);
  assert.ok(sleep > 2_000_000n);
  // Firmware path: if (sleepUs > 2000000) resync — not an infinite delay.
});
