import assert from "node:assert/strict";
import test from "node:test";
import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import {
  diffNewVolumes,
  hasEsp2Tree,
  selectMscVolume,
  waitForMscVolume,
  rejectSizeOnlyIdentity,
  volumeIdentityKey,
  formatVolumeSummary,
} from "./esp2-sd-msc-volume.mjs";
import {
  buildSdBackupForm,
  buildSdRestoreForm,
  SD_UX_ACCESS_DEVICE,
  SD_UX_ACCESS_ADVANCED,
} from "./esp2-sd-backup-ux.mjs";

function vol(root, extra = {}) {
  return { root, letter: root[0], capacityBytes: extra.capacityBytes, ...extra };
}

test("diffNewVolumes returns only newly appeared roots", () => {
  const before = [vol("E:\\"), vol("C:\\")];
  const after = [vol("E:\\"), vol("C:\\"), vol("F:\\")];
  const neu = diffNewVolumes(before, after);
  assert.equal(neu.length, 1);
  assert.equal(volumeIdentityKey(neu[0]), "F:");
});

test("backup: one new volume with esp2 → auto-select", () => {
  const root = mkdtempSync(join(tmpdir(), "esp2-vol-"));
  mkdirSync(join(root, "esp2"), { recursive: true });
  const r = selectMscVolume({
    before: [vol("C:\\")],
    after: [vol("C:\\"), vol(root)],
    mode: "backup",
  });
  assert.equal(r.status, "auto");
  assert.equal(r.selected.root, root);
  rmSync(root, { recursive: true, force: true });
});

test("backup: new volume without esp2 → reject", () => {
  const root = mkdtempSync(join(tmpdir(), "esp2-vol-empty-"));
  const r = selectMscVolume({
    before: [],
    after: [vol(root)],
    mode: "backup",
  });
  assert.equal(r.status, "none");
  assert.equal(r.reason, "new_volume_without_esp2");
  rmSync(root, { recursive: true, force: true });
});

test("restore: new volume without esp2 → allowed (auto)", () => {
  const root = mkdtempSync(join(tmpdir(), "esp2-vol-restore-"));
  const r = selectMscVolume({
    before: [],
    after: [vol(root)],
    mode: "restore",
  });
  assert.equal(r.status, "auto");
  assert.equal(hasEsp2Tree(root), false);
  rmSync(root, { recursive: true, force: true });
});

test("multiple new volumes → ambiguous selection required", () => {
  const a = mkdtempSync(join(tmpdir(), "esp2-vol-a-"));
  const b = mkdtempSync(join(tmpdir(), "esp2-vol-b-"));
  mkdirSync(join(a, "esp2"), { recursive: true });
  mkdirSync(join(b, "esp2"), { recursive: true });
  const r = selectMscVolume({
    before: [],
    after: [vol(a), vol(b)],
    mode: "backup",
  });
  assert.equal(r.status, "ambiguous");
  assert.equal(r.candidates.length, 2);
  assert.equal(r.selected, null);
  rmSync(a, { recursive: true, force: true });
  rmSync(b, { recursive: true, force: true });
});

test("no new volume → none / timeout path", async () => {
  const r = selectMscVolume({
    before: [vol("C:\\")],
    after: [vol("C:\\")],
    mode: "restore",
  });
  assert.equal(r.status, "none");

  let polls = 0;
  const waited = await waitForMscVolume({
    before: [vol("C:\\")],
    mode: "restore",
    timeoutMs: 30,
    pollMs: 5,
    sleep: async () => {},
    now: () => {
      polls += 1;
      return polls * 20;
    },
    enumerate: () => [vol("C:\\")],
  });
  assert.equal(waited.status, "none");
});

test("drive size alone is never sufficient identity", () => {
  const eightGig = vol("X:\\", { capacityBytes: 8 * 1024 ** 3 });
  const kept = rejectSizeOnlyIdentity([eightGig], 8 * 1024 ** 3);
  assert.equal(kept.length, 1);
  // Selection still requires before/after novelty — size match does not auto-pick.
  const r = selectMscVolume({
    before: [eightGig],
    after: [eightGig],
    mode: "restore",
  });
  assert.equal(r.status, "none");
});

test("formatVolumeSummary includes esp2 presence", () => {
  const root = mkdtempSync(join(tmpdir(), "esp2-vol-sum-"));
  mkdirSync(join(root, "esp2", "config"), { recursive: true });
  writeFileSync(join(root, "esp2", "config", "system.json"), "{}\n");
  const s = formatVolumeSummary(vol(root, { filesystem: "FAT32", freeBytes: 1000 }));
  assert.equal(s.hasEsp2, true);
  assert.match(s.text, /esp2=yes/);
  rmSync(root, { recursive: true, force: true });
});

test("Backup/Restore forms have no required mount-root field; open has no MSC side effects", () => {
  const backup = buildSdBackupForm({ defaultBackupRoot: "C:/proj/local/sd-backups" });
  const ids = backup.fields.map((f) => f.id);
  assert.ok(ids.includes("port"));
  assert.ok(ids.includes("destRoot"));
  assert.ok(ids.includes("accessMode"));
  assert.ok(!ids.includes("source"));
  const advanced = backup.fields.find((f) => f.id === "advancedSource");
  assert.ok(advanced);
  assert.equal(advanced.required, false);
  assert.deepEqual(advanced.visibleWhen, { accessMode: SD_UX_ACCESS_ADVANCED });

  const restore = buildSdRestoreForm({
    defaultBackupRoot: "C:/proj/local/sd-backups",
    backups: [{ path: "C:/proj/local/sd-backups/t1", id: "t1", files: 1, bytes: 10 }],
  });
  const rids = restore.fields.map((f) => f.id);
  assert.ok(rids.includes("backup"));
  assert.ok(rids.includes("backupFolder"));
  assert.ok(rids.includes("port"));
  assert.ok(!rids.includes("dest"));
  assert.ok(rids.includes("advancedDest"));
  assert.ok(!rids.includes("confirm")); // confirm is post-detect, not on open

  // Forms are plain data — building them must not touch MSC counters.
  assert.equal(globalThis.__esp2MscEnterCalls ?? 0, 0);
});

test("resolveSdMountViaUx device path auto-picks new esp2 volume and releases MSC", async () => {
  const { resolveSdMountViaUx, SD_UX_ACCESS_DEVICE } = await import(
    "./esp2-sd-backup-ux.mjs"
  );
  const root = mkdtempSync(join(tmpdir(), "esp2-msc-sess-"));
  mkdirSync(join(root, "esp2"), { recursive: true });
  let entered = 0;
  let left = 0;
  const session = await resolveSdMountViaUx({
    mode: "backup",
    accessMode: SD_UX_ACCESS_DEVICE,
    port: "COM_TEST",
    projectRoot: "/proj",
    enumerate: () => {
      // after enter, volume appears
      if (entered > 0) return [{ root }, { root: "C:\\" }];
      return [{ root: "C:\\" }];
    },
    enterMsc: () => {
      entered += 1;
    },
    leaveMsc: () => {
      left += 1;
    },
    timeoutMs: 1000,
    // waitForMscVolume uses real sleep by default — inject via enumerate only;
  });
  assert.equal(entered, 1);
  assert.equal(session.mountRoot, root);
  await session.release();
  assert.equal(left, 1);
  rmSync(root, { recursive: true, force: true });
});
