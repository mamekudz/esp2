import { testArtifactColor } from "./artifact_color.test.mjs";
import { testDskPo } from "./dsk_po.test.mjs";
import { testViewport } from "./viewport.test.mjs";
import { testGameJson, testCompatibilityDb } from "./metadata.test.mjs";
import { testVirtualKeyboard, testGamepadStateShape } from "./input_model.test.mjs";

const tests = [
  ["artifact_color", testArtifactColor],
  ["dsk_po", testDskPo],
  ["viewport", testViewport],
  ["game_json", testGameJson],
  ["compatibility_db", testCompatibilityDb],
  ["virtual_keyboard", testVirtualKeyboard],
  ["gamepad_state", testGamepadStateShape],
];

let failed = 0;
for (const [name, fn] of tests) {
  try {
    fn();
    console.log(`PASS  ${name}`);
  } catch (e) {
    failed++;
    console.error(`FAIL  ${name}`);
    console.error(e);
  }
}

if (failed) {
  console.error(`\n${failed} test(s) failed`);
  process.exit(1);
}
console.log(`\nAll ${tests.length} host tests passed`);
