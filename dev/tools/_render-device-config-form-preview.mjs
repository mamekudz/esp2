/**
 * Render device:config µParameters as an HTML preview (de-DE + en-US)
 * using the real µGulp ParseParameterDeclaration / BuildParameterForm APIs.
 * Side effects: writes HTML + PNG only under .microgulp/preview/ — no device I/O.
 */
import { pathToFileURL } from "node:url";
import { readFileSync, writeFileSync, mkdirSync, existsSync } from "node:fs";
import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";

const root = join(dirname(fileURLToPath(import.meta.url)), "../..");
const outDir = join(root, ".microgulp", "preview");
mkdirSync(outDir, { recursive: true });

const tpPath = join(
  process.env.USERPROFILE || "",
  ".cursor/extensions/meinolfamekudzi.microgulp-internal-0.9.6/src/engine/TaskParameters.mjs",
);
if (!existsSync(tpPath)) {
  console.error("TaskParameters.mjs not found — cannot render native form preview");
  process.exit(1);
}

const { ParseParameterDeclaration, BuildParameterForm } = await import(
  pathToFileURL(tpPath).href
);
const mod = await import(pathToFileURL(join(root, "gulpfile.mjs")).href + "?preview=1");
const task = Object.values(mod).find(
  (v) => typeof v === "function" && v.displayName === "device:config",
);
const raw = task["\u00b5Parameters"];
const parsed = ParseParameterDeclaration(raw);
const form = BuildParameterForm(parsed.parameters, {
  title: parsed.title,
  submitLabel: parsed.submitLabel,
});

const de = JSON.parse(readFileSync(join(root, "i18x/gulp/de-DE.json"), "utf8"));
const en = JSON.parse(readFileSync(join(root, "i18x/gulp/en-US.json"), "utf8"));

function stripCtx(s) {
  return String(s ?? "").replace(/<context="[^"]+"\/>$/, "");
}
function tr(dict, s) {
  const key = String(s ?? "");
  return stripCtx(dict[key] ?? key);
}

function escapeHtml(s) {
  return String(s)
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;");
}

function renderLocale(lid, dict) {
  const title = tr(dict, form.title);
  const submit = tr(dict, form.submitLabel || "Continue");
  const rows = [];
  for (const f of form.fields) {
    const label = escapeHtml(tr(dict, f.label));
    const desc = f.description
      ? `<div class="desc">${escapeHtml(tr(dict, f.description))}</div>`
      : "";
    let control = "";
    if (f.type === "boolean") {
      control = `<label class="switch"><input type="checkbox" ${f.default ? "checked" : ""} disabled/> <span>${f.default ? "ON" : "OFF"}</span></label>`;
    } else if (f.type === "select") {
      const opts = (f.options || [])
        .map((o) => {
          const sel = String(o.value) === String(f.default) ? " selected" : "";
          return `<option${sel}>${escapeHtml(tr(dict, o.label))}</option>`;
        })
        .join("");
      control = `<select disabled>${opts}</select>`;
    } else if (f.type === "number") {
      control = `<input type="number" value="${escapeHtml(f.default ?? "")}" min="${f.min ?? ""}" max="${f.max ?? ""}" disabled/>`;
    } else {
      control = `<input type="text" value="${escapeHtml(f.default ?? "")}" disabled/>`;
    }
    const vis =
      f.visibleWhen && f.visibleWhen.action === "apply_device"
        ? ` <span class="hint">(visibleWhen action=apply_device)</span>`
        : "";
    rows.push(
      `<div class="field" data-id="${escapeHtml(f.id)}"><div class="label">${label}${vis}</div>${desc}${control}</div>`,
    );
  }
  return `<!DOCTYPE html>
<html lang="${lid}"><head><meta charset="utf-8"/>
<title>${escapeHtml(title)}</title>
<style>
  body { font-family: "Segoe UI", sans-serif; background:#1e1e1e; color:#e8e8e8; margin:0; padding:24px; }
  .card { max-width:640px; margin:0 auto; background:#2a2a2a; border:1px solid #444; border-radius:8px; padding:20px 24px 28px; }
  h1 { font-size:18px; margin:0 0 16px; font-weight:600; }
  .field { margin:0 0 14px; }
  .label { font-size:13px; margin-bottom:4px; }
  .desc { font-size:11px; color:#9a9a9a; margin-bottom:6px; }
  .hint { font-size:10px; color:#6af; }
  input[type=text], input[type=number], select { width:100%; box-sizing:border-box; padding:8px 10px; background:#1a1a1a; border:1px solid #555; color:#eee; border-radius:4px; }
  .switch { display:inline-flex; align-items:center; gap:8px; font-size:12px; }
  .actions { margin-top:20px; display:flex; justify-content:flex-end; }
  button { background:#0e639c; color:#fff; border:0; padding:8px 18px; border-radius:4px; font-size:13px; }
  .badge { font-size:11px; color:#8c8; margin-bottom:12px; }
</style></head><body>
<div class="card">
  <div class="badge">µGulp native form preview · ${lid} · device:config · NO DEVICE I/O</div>
  <h1>${escapeHtml(title)}</h1>
  ${rows.join("\n")}
  <div class="actions"><button type="button">${escapeHtml(submit)}</button></div>
</div>
</body></html>`;
}

const deHtml = renderLocale("de-DE", de);
const enHtml = renderLocale("en-US", en);
const dePath = join(outDir, "device-config-form-de-DE.html");
const enPath = join(outDir, "device-config-form-en-US.html");
writeFileSync(dePath, deHtml, "utf8");
writeFileSync(enPath, enHtml, "utf8");

const fieldIds = form.fields.map((f) => f.id);
const requiredIds = [
  "rom",
  "drive1",
  "drive2",
  "bootFromDisk",
  "startupMacro",
  "orientation",
  "color",
  "screensaverSeconds",
];
const missing = requiredIds.filter((id) => !fieldIds.includes(id));
if (missing.length) {
  console.error("MISSING FIELDS", missing);
  process.exit(2);
}

console.log("parsed.title", tr(de, form.title));
console.log("fields", fieldIds.join(", "));
console.log("wrote", dePath);
console.log("wrote", enPath);

// Best-effort PNG via Microsoft Edge headless (if present)
const edgeCandidates = [
  "C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe",
  "C:\\Program Files\\Microsoft\\Edge\\Application\\msedge.exe",
];
const edge = edgeCandidates.find((p) => existsSync(p));
const pngPath = join(outDir, "device-config-form-de-DE.png");
if (edge) {
  const r = spawnSync(
    edge,
    [
      "--headless=new",
      "--disable-gpu",
      "--window-size=720,1400",
      `--screenshot=${pngPath}`,
      pathToFileURL(dePath).href,
    ],
    { encoding: "utf8" },
  );
  if (existsSync(pngPath)) {
    console.log("screenshot", pngPath);
  } else {
    console.log("screenshot failed", r.status, r.stderr?.slice(0, 200));
  }
} else {
  console.log("Edge not found — HTML preview only");
}

writeFileSync(
  join(outDir, "device-config-form-meta.json"),
  JSON.stringify(
    {
      taskId: "device:config",
      titleDe: tr(de, form.title),
      titleEn: tr(en, form.title),
      fieldIds,
      defaults: Object.fromEntries(
        form.fields.map((f) => [f.id, f.default ?? null]),
      ),
      sideEffects: "none",
    },
    null,
    2,
  ) + "\n",
  "utf8",
);
