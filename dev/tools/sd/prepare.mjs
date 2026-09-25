/**
 * Safe SD / library tree preparation (host only).
 * NEVER formats drives. NEVER deletes unrelated files.
 */

import {
  copyFileSync,
  existsSync,
  mkdirSync,
  readdirSync,
  statSync,
  writeFileSync,
} from "node:fs";
import { dirname, join, resolve } from "node:path";

const GAME_MEDIA_EXT = new Set([
  ".dsk",
  ".do",
  ".po",
  ".nib",
  ".woz",
  ".2mg",
  ".json",
  ".png",
]);

/**
 * Build planned actions to sync a host library into target/apple2/...
 *
 * @param {{
 *   sourceLibrary: string,
 *   targetRoot: string,
 *   dryRun?: boolean,
 *   confirmOverwrite?: boolean,
 * }} opts
 */
export function prepareSdLibrary(opts) {
  const source = resolve(opts.sourceLibrary);
  const targetRoot = resolve(opts.targetRoot);
  if (!existsSync(source)) {
    throw new Error(`source library missing: ${source}`);
  }
  if (!opts.targetRoot) {
    throw new Error("explicit target required");
  }
  // Refuse obvious OS roots
  const norm = targetRoot.replace(/\\/g, "/").toLowerCase();
  if (
    norm === "c:/" ||
    norm === "c:" ||
    norm === "/" ||
    /^[a-z]:\/?$/.test(norm)
  ) {
    throw new Error(
      `refusing to use drive root as target: ${targetRoot} (use a subdirectory)`,
    );
  }

  const apple2Root = join(targetRoot, "apple2");
  const gamesSrc = join(source, "games");
  const gamesDst = join(apple2Root, "games");
  const catalogDst = join(apple2Root, "catalog");
  const diagDst = join(apple2Root, "diagnostics");

  /** @type {{ action: string, from?: string, to: string }[]} */
  const plan = [];
  plan.push({ action: "ensure_dir", to: apple2Root });
  plan.push({ action: "ensure_dir", to: gamesDst });
  plan.push({ action: "ensure_dir", to: catalogDst });
  plan.push({ action: "ensure_dir", to: diagDst });

  if (existsSync(gamesSrc) && statSync(gamesSrc).isDirectory()) {
    for (const gameId of readdirSync(gamesSrc)) {
      const srcDir = join(gamesSrc, gameId);
      if (!statSync(srcDir).isDirectory()) continue;
      const dstDir = join(gamesDst, gameId);
      plan.push({ action: "ensure_dir", to: dstDir });
      for (const f of readdirSync(srcDir)) {
        const from = join(srcDir, f);
        if (!statSync(from).isFile()) continue;
        const ext = f.includes(".") ? f.slice(f.lastIndexOf(".")).toLowerCase() : "";
        if (!GAME_MEDIA_EXT.has(ext) && f !== "game.json") continue;
        const to = join(dstDir, f);
        if (existsSync(to)) {
          plan.push({
            action: opts.confirmOverwrite ? "overwrite_file" : "skip_existing",
            from,
            to,
          });
        } else {
          plan.push({ action: "copy_file", from, to });
        }
      }
    }
  }

  const metaDir = existsSync(join(source, "metadata"))
    ? join(source, "metadata")
    : null;
  if (metaDir) {
    for (const f of readdirSync(metaDir)) {
      if (!f.endsWith(".json")) continue;
      const from = join(metaDir, f);
      const to = join(catalogDst, f);
      plan.push({
        action: existsSync(to)
          ? opts.confirmOverwrite
            ? "overwrite_file"
            : "skip_existing"
          : "copy_file",
        from,
        to,
      });
    }
  }

  if (opts.dryRun) {
    return { ok: true, dryRun: true, targetRoot, apple2Root, plan };
  }

  // Destructive overwrite requires confirmOverwrite
  for (const step of plan) {
    if (step.action === "overwrite_file" && !opts.confirmOverwrite) {
      continue;
    }
    if (step.action === "ensure_dir") {
      mkdirSync(step.to, { recursive: true });
    } else if (step.action === "copy_file" || step.action === "overwrite_file") {
      mkdirSync(dirname(step.to), { recursive: true });
      copyFileSync(step.from, step.to);
    }
  }

  writeFileSync(
    join(apple2Root, "README.txt"),
    "ESP][ library tree. Commercial media is user-supplied only.\n",
    "utf8",
  );

  return { ok: true, dryRun: false, targetRoot, apple2Root, plan };
}
