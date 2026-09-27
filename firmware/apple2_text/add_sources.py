# Extra script: portable apple2 core for physical ROM + Disk II port.
Import("env")

if env["PIOENV"] != "apple2_text":
    Return()

from pathlib import Path

root = Path(env["PROJECT_DIR"])
apple2 = root / "host" / "src" / "apple2"

# PART F1: motherboard ROM identity + text screen + key map.
# Exclude host-only machine / input_script (HostAppleIIMachine).
env.BuildSources(
    str(Path(env.subst("$BUILD_DIR")) / "apple2_core"),
    str(apple2),
    src_filter=[
        "+<*>",
        "-<ppm.cpp>",
        "-<apple2_machine.cpp>",
        "-<input_script.cpp>",
        "-<cpu_harness.cpp>",
        # disk_ii_boot.cpp: decodeSectorFromStream used by clean-room Level-4 path
        "-<gamepad_mapper.cpp>",
        "-<display_effect.cpp>",
    ],
)

print("[apple2_text] portable core + ROM F1 + Disk II from", apple2)
