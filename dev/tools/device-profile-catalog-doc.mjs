/**
 * Write docs/device/generated/profile-catalog.md from config/device/profiles/.
 * Path-only inventory — no media payloads. Used by README / docs.
 */
import { existsSync, mkdirSync, writeFileSync, readdirSync, readFileSync } from "node:fs";
import { join, dirname } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { DEMO_PROFILE_IDS } from "./device-profiles-activate.mjs";
import { APPLE2JS_SITE_URL } from "./apple2js/paths.mjs";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "../..");
const OUT_PATH = join(ROOT, "docs/device/generated/profile-catalog.md");

const CURATED_TITLES = {
  "galaxian-demo": "Galaxian",
  "little-brick-out": "Little Brick Out",
  "star-blazer": "Star Blazer",
};

const CURATED_MEDIA_URL = {
  "galaxian-demo": `${APPLE2JS_SITE_URL}json/disks/galaxian.json`,
  "star-blazer": `${APPLE2JS_SITE_URL}json/disks/starblazer.json`,
  "little-brick-out": "", // not on apple2js website catalog
};

/**
 * @param {object} raw profile system.json
 * @param {string} id
 */
function mediaUrlForProfile(raw, id) {
  if (CURATED_MEDIA_URL[id] != null) return CURATED_MEDIA_URL[id];
  const filename = String(raw?.meta?.apple2jsFilename || "").trim();
  if (filename) {
    return `${APPLE2JS_SITE_URL}${filename.replace(/^\//, "")}`;
  }
  const base = String(raw?.meta?.apple2jsBasename || "").trim();
  if (base) return `${APPLE2JS_SITE_URL}json/disks/${base}.json`;
  return "";
}

/**
 * @returns {{ id: string, title: string, category: string, drive1: string, curated: boolean, mediaUrl: string }[]}
 */
export function collectProfileCatalogRows(root = ROOT) {
  const dir = join(root, "config/device/profiles");
  if (!existsSync(dir)) return [];
  const ids = readdirSync(dir, { withFileTypes: true })
    .filter((d) => d.isDirectory())
    .map((d) => d.name)
    .sort();
  return ids.map((id) => {
    const systemPath = join(dir, id, "system.json");
    let raw = {};
    try {
      raw = JSON.parse(readFileSync(systemPath, "utf8"));
    } catch {
      /* ignore */
    }
    const curated = DEMO_PROFILE_IDS.includes(id);
    return {
      id,
      title:
        String(raw?.meta?.title || CURATED_TITLES[id] || id).trim() || id,
      category: String(
        raw?.meta?.category || (curated ? "Demo (curated)" : ""),
      ).trim(),
      drive1: String(raw?.media?.drive1 || "").trim(),
      curated,
      mediaUrl: mediaUrlForProfile(raw, id),
    };
  });
}

/**
 * @param {{ root?: string, outPath?: string }} [opts]
 */
export function writeProfileCatalogMarkdown(opts = {}) {
  const root = opts.root || ROOT;
  const outPath = opts.outPath || OUT_PATH;
  const rows = collectProfileCatalogRows(root);
  const byCat = new Map();
  for (const r of rows) {
    const c = r.category || "(uncategorized)";
    if (!byCat.has(c)) byCat.set(c, []);
    byCat.get(c).push(r);
  }

  const lines = [];
  lines.push("# ESP][ device profile catalog");
  lines.push("");
  lines.push(
    "Generated from `config/device/profiles/`. **Path-only** - no disk images in Git.",
  );
  lines.push("");
  lines.push(
    "Each catalog profile points at an [Apple ][js](https://www.scullinsteel.com/apple2) media URL (JSON disk). Download into gitignored `local/apple2/` with:",
  );
  lines.push("");
  lines.push("```bash");
  lines.push("npx gulp setup:profiles");
  lines.push("npx gulp setup:profiles --profile star-blazer --seed star-blazer");
  lines.push("```");
  lines.push("");
  lines.push(
    "Commercial / entertainment titles remain **USER_SUPPLIED_ONLY / LOCAL_TEST_ONLY** - website availability is not redistribution permission.",
  );
  lines.push("");
  lines.push(
    `Profiles: **${rows.length}**. Regenerate: \`gulp device:profiles:generate\` or \`node dev/tools/device-profile-catalog-doc.mjs\`.`,
  );
  lines.push("");
  lines.push("## Categories");
  lines.push("");
  lines.push("| Category | Count |");
  lines.push("| --- | ---: |");
  for (const [cat, list] of [...byCat.entries()].sort((a, b) =>
    a[0].localeCompare(b[0]),
  )) {
    lines.push(`| ${cat} | ${list.length} |`);
  }
  lines.push("");
  lines.push("## Curated demos");
  lines.push("");
  lines.push("| Title | Profile id | Device path | Apple ][js media URL |");
  lines.push("| --- | --- | --- | --- |");
  for (const id of DEMO_PROFILE_IDS) {
    const r = rows.find((x) => x.id === id);
    if (!r) continue;
    const url = r.mediaUrl || "(user-supplied / not on website catalog)";
    lines.push(
      `| ${r.title} | \`${r.id}\` | \`${r.drive1 || ""}\` | ${url} |`,
    );
  }
  lines.push("");
  lines.push("## All profiles (by category)");
  lines.push("");
  for (const [cat, list] of [...byCat.entries()].sort((a, b) =>
    a[0].localeCompare(b[0]),
  )) {
    lines.push(`### ${cat}`);
    lines.push("");
    lines.push("| Title | Profile id | Apple ][js media URL |");
    lines.push("| --- | --- | --- |");
    for (const r of list.sort((a, b) => a.title.localeCompare(b.title))) {
      const url = r.mediaUrl || "-";
      lines.push(`| ${r.title} | \`${r.id}\` | ${url} |`);
    }
    lines.push("");
  }

  mkdirSync(dirname(outPath), { recursive: true });
  writeFileSync(outPath, `${lines.join("\n")}\n`, "utf8");
  return { outPath, count: rows.length };
}

if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) {
  const r = writeProfileCatalogMarkdown();
  console.log(JSON.stringify({ ok: true, ...r }, null, 2));
}
