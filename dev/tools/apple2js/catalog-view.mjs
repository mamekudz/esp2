/**
 * Human-readable apple2js catalog table / accordion helpers.
 */

/**
 * @param {object} a audited entry
 */
function sourceLabel(a) {
  const git = Boolean(a.inGitRepo);
  const web = Boolean(a.onWebsite);
  if (git && web) return "git+web";
  if (git) return "git";
  if (web) return "website";
  return a.catalogSource || "—";
}

/**
 * @param {object[]} audited auditCatalog() rows
 * @returns {{ Title: string, Category: string, Id: string, Source: string, Rights: string }[]}
 */
export function catalogTableRows(audited) {
  return (audited || [])
    .slice()
    .sort((a, b) => {
      const ca = String(a.category || "").localeCompare(String(b.category || ""));
      if (ca !== 0) return ca;
      return String(a.title || a.name || "").localeCompare(
        String(b.title || b.name || ""),
      );
    })
    .map((a) => ({
      Title: a.title || a.name || a.basename || a.id || "—",
      Category: a.category || "—",
      Id: a.id || a.basename || "—",
      Source: sourceLabel(a),
      Rights: a.redistribution?.status || "UNKNOWN",
    }));
}

/**
 * @param {object[]} rows
 * @param {Function} [log]
 */
export function printCatalogTable(rows, log = console.log) {
  if (!rows.length) {
    log("[CATALOG] empty");
    return;
  }
  const cols = ["Title", "Category", "Id", "Source", "Rights"];
  const widths = cols.map((c) =>
    Math.max(c.length, ...rows.map((r) => String(r[c] ?? "").length)),
  );
  const line = (cells) =>
    cells.map((v, i) => String(v).padEnd(widths[i])).join("  ");
  log(line(cols));
  log(widths.map((w) => "-".repeat(w)).join("  "));
  for (const r of rows) log(line(cols.map((c) => r[c])));
}

/**
 * Accordion payload for µGulp LogAccordion.
 * @param {object[]} audited
 * @param {{ title?: string, summary?: object }} [opts]
 */
export function buildCatalogAccordion(audited, opts = {}) {
  const byCat = new Map();
  for (const a of audited || []) {
    const cat = a.category || "(uncategorized)";
    if (!byCat.has(cat)) byCat.set(cat, []);
    byCat.get(cat).push(a);
  }
  const items = [...byCat.entries()]
    .sort((a, b) => a[0].localeCompare(b[0]))
    .map(([cat, list], index) => {
      const sorted = list.slice().sort((a, b) =>
        String(a.title || a.name || "").localeCompare(
          String(b.title || b.name || ""),
        ),
      );
      return {
        summary: `${cat} (${sorted.length})`,
        open: cat === "Game" || index === 0,
        lines: sorted.map((a) => {
          const title = a.title || a.name || a.basename;
          const id = a.id || a.basename;
          const rights = a.redistribution?.status || "UNKNOWN";
          return `${title}  ·  ${id}  ·  ${sourceLabel(a)}  ·  ${rights}`;
        }),
      };
    });
  return {
    title: opts.title || "Apple ][js online catalog",
    items,
    entryCount: (audited || []).length,
    summary: opts.summary || null,
  };
}
