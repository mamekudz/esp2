/**
 * Controlled failure + error-shape tests for PlatformIO / µGulp diagnostics.
 * Does not flash or touch the device.
 */
import test from "node:test";
import assert from "node:assert/strict";
import {
  Pio,
  PioExecutable,
  makeTaskError,
  formatThrownError,
  firstPioFailureLine,
} from "./pio.mjs";
import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "../..");

test("firstPioFailureLine prefers fatal error line", () => {
  const text = [
    "Compiling foo.cpp.o",
    "In file included from a.hpp:1,",
    "include/esp2_bluetooth_config.hpp:11:10: fatal error: esp_bracket/local_control.hpp: No such file or directory",
    "compilation terminated.",
  ].join("\n");
  assert.match(firstPioFailureLine(text), /fatal error:.*local_control\.hpp/);
});

test("makeTaskError JSON keeps enumerable message (contract for hosts that JSON.stringify)", () => {
  const err = makeTaskError("pio build failed with exit code 1: fatal error: missing.hpp", {
    code: 1,
    detail: "fatal error: missing.hpp",
  });
  const json = JSON.stringify(err);
  assert.match(json, /pio build failed/);
  assert.match(json, /missing\.hpp/);
  assert.notEqual(json, "{}");
});

test("µGulp RunLogger-style String(errorObject) is the [object Object] bug", () => {
  // Mirrors JetBrains microgulp RunLogger.FinishRun:
  //   error: { message, stack } from TaskWorker
  //   then: String(_result.error)  → "[object Object]"
  const taskWorkerPayload = {
    message: "pio build failed with exit code 1",
    stack: "Error: pio build failed with exit code 1\n    at …",
  };
  assert.equal(String(taskWorkerPayload), "[object Object]");
  // Correct contract: use .message (or formatThrownError).
  assert.equal(formatThrownError(taskWorkerPayload), "pio build failed with exit code 1");
});

test("controlled PlatformIO failure: Pio throws Error with human message (no flash)", async () => {
  // Invalid env — safe, no upload, no device touch.
  await assert.rejects(
    () => Pio(ROOT, ["run", "-e", "esp2_no_such_env_zz"], "pio controlled-fail"),
    (err) => {
      assert.ok(err instanceof Error);
      assert.match(String(err.message), /failed with exit code/);
      assert.notEqual(String(err.message), "[object Object]");
      assert.notEqual(formatThrownError(err), "[object Object]");
      // Simulated RunLogger footer if host used message correctly:
      const footer = `# error: ${formatThrownError(err)}`;
      assert.equal(footer.includes("[object Object]"), false);
      return true;
    },
  );
});

test("PioExecutable resolves", () => {
  assert.ok(PioExecutable().length > 0);
});
