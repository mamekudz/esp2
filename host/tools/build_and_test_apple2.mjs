/**
 * Build and run host Apple II C++ tests (requires tools/host-toolchain LLVM-MinGW).
 *
 * Usage: node host/tools/build_and_test_apple2.mjs
 */
import { spawnSync } from "node:child_process";
import { existsSync, mkdirSync, readdirSync } from "node:fs";
import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";

const root = join(dirname(fileURLToPath(import.meta.url)), "../..");
const toolchainBin = join(
  root,
  "tools/host-toolchain/llvm-mingw-20240619-ucrt-x86_64/bin",
);
const gpp = join(toolchainBin, "g++.exe");
const gcc = join(toolchainBin, "gcc.exe");
const outDir = join(root, "host/.out");

function fail(msg) {
  console.error(msg);
  process.exit(1);
}

if (!existsSync(gpp) || !existsSync(gcc)) {
  fail(
    `Host toolchain not found at ${toolchainBin}\n` +
      "Extract LLVM-MinGW ucrt x86_64 there (see docs/apple2/core-evaluation.md).",
  );
}

mkdirSync(outDir, { recursive: true });

const cppSources = readdirSync(join(root, "host/src/apple2"))
  .filter((f) => f.endsWith(".cpp"))
  .map((f) => join(root, "host/src/apple2", f));

const cSources = readdirSync(join(root, "host/src/apple2"))
  .filter((f) => f.endsWith(".c"))
  .map((f) => join(root, "host/src/apple2", f));

const env = { ...process.env, PATH: `${toolchainBin};${process.env.PATH || ""}` };

function run(cmd, args) {
  const r = spawnSync(cmd, args, { cwd: root, env, encoding: "utf8" });
  if (r.stdout) process.stdout.write(r.stdout);
  if (r.stderr) process.stderr.write(r.stderr);
  if (r.status !== 0) {
    fail(`Command failed (${r.status}): ${cmd} ${args.join(" ")}`);
  }
}

const objs = [];
for (const src of cSources) {
  const obj = join(outDir, src.split(/[/\\]/).pop().replace(/\.c$/, ".o"));
  run(gcc, ["-std=c11", "-O2", "-Ithird_party/fake6502", "-c", src, "-o", obj]);
  objs.push(obj);
}
for (const src of cppSources) {
  const obj = join(outDir, src.split(/[/\\]/).pop().replace(/\.cpp$/, ".o"));
  run(gpp, [
    "-std=c++17",
    "-O2",
    "-Wall",
    "-Wextra",
    "-Ihost/include",
    "-Ihost/src/apple2",
    "-Ithird_party/fake6502",
    "-c",
    src,
    "-o",
    obj,
  ]);
  objs.push(obj);
}

function buildTest(name, mainCpp) {
  const mainObj = join(outDir, `${name}_main.o`);
  run(gpp, [
    "-std=c++17",
    "-O2",
    "-Wall",
    "-Wextra",
    "-Ihost/include",
    "-Ihost/src/apple2",
    "-c",
    mainCpp,
    "-o",
    mainObj,
  ]);
  const exe = join(outDir, `${name}.exe`);
  run(gpp, ["-o", exe, ...objs, mainObj]);
  console.log(`\n=== RUN ${name} ===`);
  run(exe, [outDir]);
}

buildTest("test_cpu", join(root, "host/test_cpp/test_cpu_main.cpp"));
buildTest("test_machine", join(root, "host/test_cpp/test_machine_main.cpp"));
buildTest("test_video", join(root, "host/test_cpp/test_video.cpp"));
buildTest("test_io_page", join(root, "host/test_cpp/test_io_page.cpp"));
buildTest("test_disk_ii", join(root, "host/test_cpp/test_disk_ii.cpp"));
buildTest("test_level3", join(root, "host/test_cpp/test_level3.cpp"));
buildTest("test_speaker", join(root, "host/test_cpp/test_speaker.cpp"));
buildTest("test_level4", join(root, "host/test_cpp/test_level4.cpp"));
buildTest("test_perf", join(root, "host/test_cpp/test_perf.cpp"));
buildTest("test_video_dirty", join(root, "host/test_cpp/test_video_dirty.cpp"));
buildTest("test_landscape_present", join(root, "host/test_cpp/test_landscape_present.cpp"));

// Build interactive console + esp2_host + boot_forensic + sst_undoc_runner
// (do not run interactive / media / optional SST here)
{
  const mains = [
    ["host_console", join(root, "host/test_cpp/host_console.cpp")],
    ["esp2_host", join(root, "host/test_cpp/esp2_host.cpp")],
    ["boot_forensic", join(root, "host/test_cpp/boot_forensic.cpp")],
    ["sst_undoc_runner", join(root, "host/test_cpp/sst_undoc_runner.cpp")],
    ["rom_data_forensic", join(root, "host/test_cpp/rom_data_forensic.cpp")],
  ];
  for (const [name, mainCpp] of mains) {
    if (!existsSync(mainCpp)) continue;
    const mainObj = join(outDir, `${name}_main.o`);
    run(gpp, [
      "-std=c++17",
      "-O2",
      "-Wall",
      "-Wextra",
      "-Ihost/include",
      "-Ihost/src/apple2",
      "-c",
      mainCpp,
      "-o",
      mainObj,
    ]);
    run(gpp, ["-o", join(outDir, `${name}.exe`), ...objs, mainObj]);
    console.log(`\nBuilt host/.out/${name}.exe`);
  }
}


console.log("\nHost Apple II C++ suite OK");
