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
import {
  testSourcesRegistry,
  testProvenanceGates,
  testApple2jsSyncDiffBlocksUpgrade,
  testFormatMatrixAndIdentify,
  testImportMultiDiskAndDuplicates,
  testDirectoryImportNonRecursive,
  testSdPrepareDryRunSafety,
  testWozRedistributableHash,
} from "./media_catalog.test.mjs";

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
  ["sources_registry", testSourcesRegistry],
  ["provenance_gates", testProvenanceGates],
  ["apple2js_sync_diff", testApple2jsSyncDiffBlocksUpgrade],
  ["format_identify", testFormatMatrixAndIdentify],
  ["import_multidisk_dupes", testImportMultiDiskAndDuplicates],
  ["directory_import", testDirectoryImportNonRecursive],
  ["sd_prepare_dryrun", testSdPrepareDryRunSafety],
  ["woz_redistributable", testWozRedistributableHash],
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
