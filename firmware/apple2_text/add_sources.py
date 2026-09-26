# Extra script: portable apple2 core for physical video + Disk II port.
Import("env")

if env["PIOENV"] != "apple2_text":
    Return()

from pathlib import Path

root = Path(env["PROJECT_DIR"])
apple2 = root / "host" / "src" / "apple2"

# PART E: include Disk II + SHA-256; exclude host-only / DisplayEffect / Machine host.
env.BuildSources(
    str(Path(env.subst("$BUILD_DIR")) / "apple2_core"),
    str(apple2),
    src_filter=[
        "+<*>",
        "-<ppm.cpp>",
        "-<apple2_machine.cpp>",
        "-<input_script.cpp>",
        "-<cpu_harness.cpp>",
        "-<disk_ii_boot.cpp>",  # host sector shortcut — not used on ESP32
        "-<gamepad_mapper.cpp>",
        "-<key_map.cpp>",
        "-<rom_identity.cpp>",
        "-<text_screen.cpp>",
        "-<display_effect.cpp>",
    ],
)

print("[apple2_text] portable core + Disk II + artifact from", apple2)
