# Extra script: portable apple2 core for physical ROM + Disk II port.
Import("env")

pioenv = env["PIOENV"]
if pioenv not in ("apple2_text", "diymore_apple2", "rlcd42_apple2"):
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
        # gamepad_mapper.cpp: BLE HID → GameIo (native path)
        # display_effect.cpp: CRT/TV RGB565 pass (presentation only)
    ],
)

print("[%s] portable core + ROM F1 + Disk II from %s" % (pioenv, apple2))
