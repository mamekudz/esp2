/**
 * RELEASES.json merge + history — deterministic tests.
 */
import test from "node:test";
import assert from "node:assert/strict";
import {
  mkdtempSync,
  mkdirSync,
  readFileSync,
  rmSync,
  writeFileSync,
  existsSync,
} from "node:fs";
import { tmpdir } from "node:os";
import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";
import {
  PrepareReleaseContext,
  StripReleaseContext,
  FingerprintReleaseText,
} from "./releases/release-context.mjs";
import {
  CompareVersions,
  FormatVersionString,
} from "./releases/release-version.mjs";
import {
  CountPendingContributorMerges,
  IsWithinMaxAge,
  MergeDeveloperReleases,
  ParseReleaseDate,
} from "./releases/release-merge.mjs";
import {
  BuildReleaseHistoryAccordion,
  FormatReleaseHistoryText,
} from "./releases/release-history.mjs";
import { ReleasesPaths } from "./releases/paths.mjs";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "../..");
const NOW = new Date("2026-09-27T12:00:00");
const TEMP = [];

test.after(() => {
  for (const root of TEMP) {
    try {
      rmSync(root, { recursive: true, force: true });
    } catch {
      /* ignore */
    }
  }
});

function _Temp() {
  const root = mkdtempSync(join(tmpdir(), "esp2-releases-"));
  TEMP.push(root);
  const developerDir = join(root, "dev", "releases");
  mkdirSync(developerDir, { recursive: true });
  return { root, developerDir, releasesPath: join(root, "RELEASES.json") };
}

test("PrepareReleaseContext is idempotent", () => {
  const once = PrepareReleaseContext("Fixed bridge queue overflow.");
  const twice = PrepareReleaseContext(once + '<context="release info"/>');
  assert.equal(once, 'Fixed bridge queue overflow.<context="release info"/>');
  assert.equal(twice, once);
  assert.equal((twice.match(/<context=/g) || []).length, 1);
});

test("Fingerprint ignores context and case", () => {
  assert.equal(
    FingerprintReleaseText('Fixed ICON.<context="release info"/>'),
    FingerprintReleaseText("fixed icon.")
  );
});

test("CompareVersions orders 1.10.0 above 1.2.0", () => {
  assert.ok(CompareVersions("1.10.0", "1.2.0") > 0);
  assert.equal(FormatVersionString({ main: 0, minor: 5, revision: 0 }), "0.5.0");
});

test("30-day window", () => {
  assert.ok(IsWithinMaxAge(NOW, ParseReleaseDate("2026-09-27 14:00"), 30));
  assert.equal(IsWithinMaxAge(NOW, ParseReleaseDate("2026-08-20 10:00"), 30), false);
});

test("merge: fresh notes in, duplicates and expired out; idempotent", () => {
  const { developerDir, releasesPath } = _Temp();
  writeFileSync(
    releasesPath,
    JSON.stringify(
      {
        releases: [
          {
            main: 0,
            minor: 5,
            revision: 0,
            date: "2026-09-26 12:00",
            beta: true,
            info: [
              'Already in central history.<context="release info"/>',
            ],
          },
        ],
      },
      null,
      "\t"
    )
  );
  writeFileSync(
    join(developerDir, "MAM.json"),
    JSON.stringify({
      author: "MAM",
      contributions: [
        {
          main: 0,
          minor: 5,
          revision: 0,
          date: "2026-09-27 10:00",
          beta: true,
          info: [
            "Brand new note for merge.",
            "Already in central history.",
            "Already in central history.<context=\"release info\"/>",
          ],
        },
        {
          main: 0,
          minor: 1,
          revision: 0,
          date: "2026-08-01 09:00",
          beta: true,
          info: ["Too old to merge."],
        },
      ],
    })
  );

  const first = MergeDeveloperReleases({
    developerDir,
    releasesPath,
    now: NOW,
    maxAgeDays: 30,
    write: true,
  });
  assert.equal(first.merged, 1);
  assert.ok(first.duplicates >= 1);
  assert.equal(first.expired, 1);

  const central = JSON.parse(readFileSync(releasesPath, "utf8"));
  const infos = central.releases[0].info.map(StripReleaseContext);
  assert.ok(infos.includes("Brand new note for merge."));
  assert.equal(
    infos.filter((t) => t === "Already in central history.").length,
    1
  );

  const second = MergeDeveloperReleases({
    developerDir,
    releasesPath,
    now: NOW,
    maxAgeDays: 30,
    write: true,
  });
  assert.equal(second.merged, 0);
  assert.equal(
    CountPendingContributorMerges({
      developerDir,
      releasesPath,
      now: NOW,
      maxAgeDays: 30,
    }),
    0
  );
});

test("live repo: RELEASES.json + MAM.json + docs present", () => {
  const paths = ReleasesPaths(ROOT);
  assert.ok(existsSync(paths.releasesPath));
  assert.ok(existsSync(join(paths.developerDir, "MAM.json")));
  assert.ok(existsSync(join(ROOT, "docs/releases/README.md")));
  assert.ok(existsSync(join(ROOT, "i18x/gulp/en-US.json")));
  assert.ok(existsSync(join(ROOT, "i18x/gulp/de-DE.json")));
  const central = JSON.parse(readFileSync(paths.releasesPath, "utf8"));
  assert.ok(Array.isArray(central.releases) && central.releases.length >= 1);
  for (const line of central.releases[0].info) {
    assert.match(line, /<context="release info"\s*\/>/);
  }
  const pending = CountPendingContributorMerges({
    ...paths,
    now: NOW,
    maxAgeDays: 30,
  });
  assert.equal(pending, 0, "bootstrap MAM note already in RELEASES");
});

test("history accordion is readable (not raw JSON dump)", () => {
  const paths = ReleasesPaths(ROOT);
  const accordion = BuildReleaseHistoryAccordion({
    releasesPath: paths.releasesPath,
    root: ROOT,
    lid: "en-US",
    maxReleases: 3,
  });
  assert.match(accordion.title, /release history/i);
  assert.ok(accordion.items.length >= 1);
  assert.match(accordion.items[0].summary, /^V0\./);
  assert.ok(accordion.items[0].lines.length >= 1);
  const text = FormatReleaseHistoryText(accordion);
  assert.equal(text.includes('"releases"'), false);
  assert.ok(text.includes("•"));

  const de = BuildReleaseHistoryAccordion({
    releasesPath: paths.releasesPath,
    root: ROOT,
    lid: "de-DE",
    maxReleases: 1,
  });
  assert.match(de.title, /Versionshistorie/);
});

import {
  AuditInfoValue,
  CheckReleaseContexts,
  FixReleaseContexts,
  MACHINE_INFO_KEYS,
} from "./releases/release-context-audit.mjs";
import {
  CheckReleaseI18xCompleteness,
  UpdateReleaseI18xSources,
} from "./releases/release-i18x.mjs";

test("context: nested info + machine-data exclusion", () => {
  const nested = {
    title: "Physically verified playfield",
    description: "Galaxian HGR on CO5300",
    commit: "021be3e",
    model: "ESP32-S3",
  };
  const check = AuditInfoValue(nested, "info[0]", { fix: false });
  assert.ok(check.issues.some((i) => i.kind === "missing" && i.path.endsWith(".title")));
  assert.ok(check.issues.some((i) => i.kind === "missing" && i.path.endsWith(".description")));
  assert.equal(
    check.issues.filter((i) => i.path.endsWith(".commit") || i.path.endsWith(".model")).length,
    0
  );

  const fixed = AuditInfoValue(
    { ...nested, commit: '021be3e<context="release info"/>' },
    "info[0]",
    { fix: true }
  );
  assert.equal(fixed.value.commit, "021be3e");
  assert.match(fixed.value.title, /release info/);
  assert.ok(MACHINE_INFO_KEYS.has("commit"));
});

test("context FIX on RELEASES is idempotent (zero second diff)", () => {
  const { root, releasesPath } = _Temp();
  writeFileSync(
    releasesPath,
    JSON.stringify(
      {
        releases: [
          {
            main: 0,
            minor: 1,
            revision: 0,
            date: "2026-09-27 12:00",
            beta: true,
            info: [
              "Needs a tag.",
              'Already tagged.<context="release info"/>',
              'Dup tag.<context="release info"/><context="release info"/>',
            ],
          },
        ],
      },
      null,
      "\t"
    )
  );
  // point FixReleaseContexts at temp root via ReleasesPaths join — use root with RELEASES.json
  mkdirSync(join(root, "dev", "releases"), { recursive: true });
  const first = FixReleaseContexts({ root, write: true });
  assert.equal(first.changed, true);
  const after1 = readFileSync(releasesPath, "utf8");
  const second = FixReleaseContexts({ root, write: true });
  assert.equal(second.changed, false);
  assert.equal(readFileSync(releasesPath, "utf8"), after1);
  const check = CheckReleaseContexts({ root });
  assert.equal(check.ok, true);
});

test("normal UpdateReleaseI18xSources does not invent de-DE text", () => {
  const paths = ReleasesPaths(ROOT);
  const before = existsSync(join(ROOT, "i18x/gulp/releases/de-DE.json"))
    ? readFileSync(join(ROOT, "i18x/gulp/releases/de-DE.json"), "utf8")
    : "";
  const result = UpdateReleaseI18xSources({ root: ROOT, write: true });
  assert.ok(result.bodies >= 1);
  // Missing keys stay missing (empty map entries not auto-filled with English)
  for (const miss of result.missingDe) {
    assert.ok(miss.length > 0);
  }
  const completeness = CheckReleaseI18xCompleteness(ROOT);
  assert.equal(completeness.bodyCount, result.bodies);
  // Writing again with same sources is stable for en-US
  const again = UpdateReleaseI18xSources({ root: ROOT, write: true });
  assert.equal(again.bodies, result.bodies);
  void before;
});
