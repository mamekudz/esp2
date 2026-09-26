# Extra script: portable apple2 core for physical text-port env.
Import("env")

if env["PIOENV"] != "apple2_text":
    Return()

from pathlib import Path

root = Path(env["PROJECT_DIR"])
apple2 = root / "host" / "src" / "apple2"

# Exclude host-only / heavy Disk II / machine façade for this text-only milestone.
env.BuildSources(
    str(Path(env.subst("$BUILD_DIR")) / "apple2_core"),
    str(apple2),
    src_filter=[
        "+<*>",
        "-<ppm.cpp>",
        "-<apple2_machine.cpp>",
        "-<input_script.cpp>",
        "-<cpu_harness.cpp>",
        "-<disk_ii_*.cpp>",
        "-<gamepad_mapper.cpp>",
        "-<key_map.cpp>",
        "-<rom_identity.cpp>",
        "-<sha256.cpp>",
        "-<text_screen.cpp>",
        "-<artifact_renderer.cpp>",
        "-<hgr_decoder.cpp>",
        "-<lores_decoder.cpp>",
        "-<display_effect.cpp>",
        "-<video_state.cpp>",
    ],
)

print("[apple2_text] portable core sources from", apple2)
