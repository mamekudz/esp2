# Contributor release notes (ESP][)

Author notes here in **en-US** only.

## Workflow

1. Add entries under `contributions` in your initials file (e.g. `MAM.json`).
2. Do **not** add `<context="release info"/>` — `releases:update` owns that.
3. Run `gulp releases:update` (or `npm run releases:update`).
4. Only notes **newer than 30 days** and **not already** in root `RELEASES.json` merge.

Duplicate identity = `version` + fingerprint of stripped info text (not array position).

See `docs/releases/README.md`.
