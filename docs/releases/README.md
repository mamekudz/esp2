# ESP][ — RELEASES.json workflow

**Scope:** project history / µGulp release tasks only.

---

## Purpose

Root `RELEASES.json` is the **canonical consolidated** ESP][ project history (en-US).

Contributor files under `dev/releases/*.json` avoid merge conflicts.

| File | Role |
| --- | --- |
| `RELEASES.json` | Canonical history (newest first) |
| `dev/releases/<INITIALS>.json` | Authoring source (en-US) |
| `i18x/gulp/releases/*.json` | Optional de-DE (and en-US) translations of info bodies |
| `i18x/gulp/*.json` | µGulp task display names / descriptions |

---

## Authoring

1. Write notes in **en-US** only.
2. Do **not** manually maintain `<context="release info"/>` in contributor files.
3. Use `{ main, minor, revision, date, beta, info[] }`.
4. Run `gulp releases:update`.

Normal development does **not** auto-translate.

---

## 30-day merge

Contributor entries merge only if:

- `date` is within the last **30 days**, and
- the info line is **not already** in `RELEASES.json`

Identity: `version` + fingerprint(stripped text). Re-run is **idempotent**.

---

## Context tags

Canonical lines in `RELEASES.json` end with `<context="release info"/>`.

| Task | Role |
| --- | --- |
| `releases:context-check` | Detect missing / duplicate / malformed / machine-tagged context (**read-only**) |
| `releases:context-fix` | Normalize tags on `RELEASES.json` (idempotent) |

Machine/identity values (`commit`, `sha`, `url`, …) are never tagged.

---

## Explicit i18x

`gulp releases:i18x-update` extracts en-US bodies and preserves existing de-DE translations.
It reports missing German strings — it does **not** invent AI translations during normal use.

---

## µGulp tasks (Docs group)

| Technical ID | Role |
| --- | --- |
| `releases:update` | Merge contributor notes → `RELEASES.json` |
| `releases:history` | Localized readable history |
| `releases:context-check` | Context CHECK |
| `releases:context-fix` | Context FIX |
| `releases:i18x-update` | Extract / report release i18x |

---

## Pre-publication order

```
releases:update
  → releases:context-check
  → releases:context-fix (if needed)
  → releases:context-check
  → releases:i18x-update
  → verify de-DE completeness
  → publication may continue
```

---

## Verification language

Preserve evidence labels: `PHYSICALLY_VERIFIED` · `HOST_VERIFIED` · `IMPLEMENTED` · `NOT_TESTED` · `DOCUMENTED`.
