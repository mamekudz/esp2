# Extra script: add portable host apple2 sources to core_smoke env.
Import("env")

if env["PIOENV"] != "core_smoke":
    Return()

from pathlib import Path

root = Path(env["PROJECT_DIR"])
apple2 = root / "host" / "src" / "apple2"

env.BuildSources(
    str(Path(env.subst("$BUILD_DIR")) / "apple2_core"),
    str(apple2),
    src_filter=[
        "+<*>",
        "-<ppm.cpp>",
        "-<apple2_machine.cpp>",
        "-<input_script.cpp>",
        "-<cpu_harness.cpp>",
    ],
)

print("[core_smoke] portable apple2 sources from", apple2)
