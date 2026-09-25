/**
 * Infrastructure checks for ESP][ µGulp helpers (no real NAS / no Git commit).
 */
import test from 'node:test';
import assert from 'node:assert/strict';
import { mkdtempSync, rmSync, existsSync, readFileSync } from 'node:fs';
import { join } from 'node:path';
import { tmpdir } from 'node:os';
import {
	NAS_BACKUP_INCLUDE_FILES,
	NAS_BACKUP_EXCLUDE_DIRS,
	NAS_BACKUP_MAX_DESTINATIONS,
	UniqueNasTargets,
	ResolveNasTargets,
	MirrorNonReproducible,
	VerifyBackupContents,
} from './nas-backup.mjs';
import {
	BackupCommitMessage,
	GIT_BACKUP_ENSURE_PATHS,
	GIT_BACKUP_NEVER_STAGE,
} from './git-backup.mjs';
import { ComposeReadme, FilterChannels, README_SOURCE_RELATIVE } from './readme-compose.mjs';

const ROOT = join(import.meta.dirname, '..', '..');

test('NAS max destinations is three', () => {
	assert.equal(NAS_BACKUP_MAX_DESTINATIONS, 3);
	const { destinations, rejectedExtra } = UniqueNasTargets(['a', 'b', 'c', 'd']);
	assert.deepEqual(destinations, ['a', 'b', 'c']);
	assert.equal(rejectedExtra, 1);
});

test('0 NAS targets from empty config', () => {
	const prev1 = process.env.NAS_TARGET_1;
	const prev2 = process.env.NAS_TARGET_2;
	const prev3 = process.env.NAS_TARGET_3;
	delete process.env.NAS_TARGET_1;
	delete process.env.NAS_TARGET_2;
	delete process.env.NAS_TARGET_3;
	delete process.env.ESP2_NAS_BACKUP_PATHS;
	try {
		const resolved = ResolveNasTargets(ROOT);
		assert.equal(resolved.destinations.length, 0);
		assert.ok(resolved.source === 'none' || resolved.source.includes('nas.targets'));
	} finally {
		if (prev1 !== undefined) process.env.NAS_TARGET_1 = prev1;
		else delete process.env.NAS_TARGET_1;
		if (prev2 !== undefined) process.env.NAS_TARGET_2 = prev2;
		else delete process.env.NAS_TARGET_2;
		if (prev3 !== undefined) process.env.NAS_TARGET_3 = prev3;
		else delete process.env.NAS_TARGET_3;
	}
});

test('CLAUDE.md is in Git ensure + NAS include lists', () => {
	assert.ok(GIT_BACKUP_ENSURE_PATHS.includes('CLAUDE.md'));
	assert.ok(NAS_BACKUP_INCLUDE_FILES.includes('CLAUDE.md'));
	assert.ok(existsSync(join(ROOT, 'CLAUDE.md')));
	assert.ok(GIT_BACKUP_NEVER_STAGE.includes('config/nas.targets.local'));
});

test('NAS excludes regenerable trees', () => {
	assert.ok(NAS_BACKUP_EXCLUDE_DIRS.includes('node_modules'));
	assert.ok(NAS_BACKUP_EXCLUDE_DIRS.includes('.pio'));
});

test('checkpoint message format', () => {
	assert.equal(
		BackupCommitMessage(new Date(2026, 8, 25, 13, 45)),
		'backup: ESP][ 2026-09-25 13:45'
	);
});

test('readme filter strips note/website and is deterministic', () => {
	const src =
		'<!-- note\nsecret\n-->\n# Title\n\nvisible\n\n<!-- website\ndrop\n-->\n';
	const a = FilterChannels(src, 'git');
	const b = FilterChannels(src, 'git');
	assert.equal(a, b);
	assert.match(a, /# Title/);
	assert.match(a, /visible/);
	assert.doesNotMatch(a, /secret/);
	assert.doesNotMatch(a, /drop/);
});

test('ComposeReadme is idempotent (second run unchanged)', () => {
	assert.equal(README_SOURCE_RELATIVE, 'dev/docs/readme/de-DE.src.md');
	const first = ComposeReadme({ root: ROOT });
	const second = ComposeReadme({ root: ROOT });
	assert.equal(second.changed, false);
	assert.equal(first.bytes, second.bytes);
	assert.match(readFileSync(join(ROOT, 'README.md'), 'utf8'), /ESP\]\[/);
	assert.match(readFileSync(join(ROOT, 'README.md'), 'utf8'), /microgulp-ready\.png/);
});

test('NAS mirror includes CLAUDE.md and skips .pio/node_modules', async () => {
	const dest = mkdtempSync(join(tmpdir(), 'esp2-nas-'));
	const missingTarget = join(tmpdir(), 'esp2-nas-missing-' + Date.now());
	try {
		await MirrorNonReproducible(ROOT, dest, false);
		assert.ok(existsSync(join(dest, 'CLAUDE.md')), 'CLAUDE.md copied');
		assert.ok(existsSync(join(dest, 'platformio.ini')));
		assert.ok(existsSync(join(dest, 'dev', 'docs', 'readme', 'de-DE.src.md')));
		assert.equal(existsSync(join(dest, 'node_modules')), false);
		assert.equal(existsSync(join(dest, '.pio')), false);
		const check = VerifyBackupContents(dest);
		assert.equal(check.ok, true, check.missing.join(','));

		// Unavailable path must not throw when only checking existsSync (gulpfile skips)
		assert.equal(existsSync(missingTarget), false);
	} finally {
		rmSync(dest, { recursive: true, force: true });
	}
});
