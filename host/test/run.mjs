import { testArtifactColor } from "./artifact_color.test.mjs";
import { testDskPo } from "./dsk_po.test.mjs";
import { testViewport } from "./viewport.test.mjs";
import { testGameJson, testCompatibilityDb } from "./metadata.test.mjs";
import { testVirtualKeyboard, testGamepadStateShape } from "./input_model.test.mjs";
import {
  testApple2jsCatalogParseAndNormalize,
  testApple2jsMalformedIndex,
  testApple2jsJsonDiskRoundtrip,
  testCatalogMergePrefersGitFlag,
} from "./apple2js_catalog.test.mjs";
import {
  testMediaIdentifySynthetic,
  testMediaImportAndNoMediaState,
} from "./media_import.test.mjs";

const tests = [
  ["artifact_color", testArtifactColor],
  ["dsk_po", testDskPo],
  ["viewport", testViewport],
  ["game_json", testGameJson],
  ["compatibility_db", testCompatibilityDb],
  ["virtual_keyboard", testVirtualKeyboard],
  ["gamepad_state", testGamepadStateShape],
  ["apple2js_catalog", testApple2jsCatalogParseAndNormalize],
  ["apple2js_malformed", testApple2jsMalformedIndex],
  ["apple2js_json_disk", testApple2jsJsonDiskRoundtrip],
  ["apple2js_merge", testCatalogMergePrefersGitFlag],
  ["media_identify", testMediaIdentifySynthetic],
  ["media_import", testMediaImportAndNoMediaState],
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
