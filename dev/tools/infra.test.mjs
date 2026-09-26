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
import {
	ComposeReadme,
	ComposeReadmeLocale,
	FilterChannels,
	README_SOURCE_RELATIVE,
	README_SOURCES,
	MICROGULP_READY_ASSET,
} from './readme-compose.mjs';

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
	assert.ok(NAS_BACKUP_EXCLUDE_DIRS.includes('local'));
});

test('Git backup never stages local Apple II media', () => {
	assert.ok(GIT_BACKUP_NEVER_STAGE.includes('local/apple2'));
	assert.ok(GIT_BACKUP_NEVER_STAGE.includes('library/user'));
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

test('locale README sources exist', () => {
	assert.equal(README_SOURCE_RELATIVE, 'dev/docs/readme/en-US.src.md');
	for (const locale of Object.keys(README_SOURCES)) {
		const src = join(ROOT, README_SOURCES[locale].sourceRel);
		assert.ok(existsSync(src), `missing ${src}`);
	}
});

test('ComposeReadme is idempotent (second run unchanged)', () => {
	const first = ComposeReadme({ root: ROOT });
	const second = ComposeReadme({ root: ROOT });
	assert.equal(second.changed, false);
	assert.equal(first.bytes, second.bytes);
	assert.equal(second.results.length, 2);
});

test('generated READMEs: language selector, WIP, µGulp-ready', () => {
	ComposeReadme({ root: ROOT });
	const en = readFileSync(join(ROOT, 'README.md'), 'utf8');
	const de = readFileSync(join(ROOT, 'README.de-DE.md'), 'utf8');

	assert.match(en, /ESP\]\[/);
	assert.match(de, /ESP\]\[/);

	assert.match(en, /\[Deutsch\]\(README\.de-DE\.md\)/);
	assert.match(de, /\[English\]\(README\.md\)/);

	assert.match(en, /Work in Progress/);
	assert.match(de, /In Entwicklung/);

	assert.match(en, /HOST_VERIFIED/);
	assert.match(de, /HOST_VERIFIED/);
	assert.match(en, /not yet.*integrated into the ESP32/i);
	assert.match(de, /noch nicht.*in die ESP32-Firmware/i);

	assert.match(en, /## µGulp-ready/);
	assert.match(de, /## µGulp-ready/);
	assert.match(en, /documentation generation/i);
	assert.match(de, /Dokumentationsgenerierung/);

	assert.match(en, new RegExp(MICROGULP_READY_ASSET.replace(/\./g, '\\.')));
	assert.match(de, new RegExp(MICROGULP_READY_ASSET.replace(/\./g, '\\.')));

	assert.doesNotMatch(en, /\{\{[A-Z_]+\}\}/);
	assert.doesNotMatch(de, /\{\{[A-Z_]+\}\}/);
	assert.doesNotMatch(en, /TODO_TRANSLATE/);
	assert.doesNotMatch(de, /TODO_TRANSLATE/);
});

test('canonical µGulp-ready asset exists once', () => {
	const asset = join(ROOT, MICROGULP_READY_ASSET);
	assert.ok(existsSync(asset), `missing ${MICROGULP_READY_ASSET}`);
	assert.equal(existsSync(join(ROOT, 'docs/assets/microgulp-ready-en.png')), false);
	assert.equal(existsSync(join(ROOT, 'docs/assets/microgulp-ready-de.png')), false);
	ComposeReadmeLocale({ root: ROOT, locale: 'en-US' });
	ComposeReadmeLocale({ root: ROOT, locale: 'de-DE' });
});

test('NAS mirror includes CLAUDE.md and skips .pio/node_modules', async () => {
	const dest = mkdtempSync(join(tmpdir(), 'esp2-nas-'));
	const missingTarget = join(tmpdir(), 'esp2-nas-missing-' + Date.now());
	try {
		await MirrorNonReproducible(ROOT, dest, false);
		assert.ok(existsSync(join(dest, 'CLAUDE.md')), 'CLAUDE.md copied');
		assert.ok(existsSync(join(dest, 'platformio.ini')));
		assert.ok(existsSync(join(dest, 'dev', 'docs', 'readme', 'de-DE.src.md')));
		assert.ok(existsSync(join(dest, 'dev', 'docs', 'readme', 'en-US.src.md')));
		assert.equal(existsSync(join(dest, 'node_modules')), false);
		assert.equal(existsSync(join(dest, '.pio')), false);
		const check = VerifyBackupContents(dest);
		assert.equal(check.ok, true, check.missing.join(','));

		assert.equal(existsSync(missingTarget), false);
	} finally {
		rmSync(dest, { recursive: true, force: true });
	}
});
