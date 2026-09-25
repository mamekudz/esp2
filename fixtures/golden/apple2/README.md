# Golden fixtures (Apple II host video)

Golden checks for text row bases and HGR line addresses live in the C++
host tests (`host/test_cpp/test_machine_main.cpp`).

## Updating goldens intentionally

1. Change decoder/renderer with a clear reason.
2. Run `node host/tools/build_and_test_apple2.mjs`.
3. Visually inspect `host/.out/*.ppm` (gitignored).
4. Update asserted constants **only** in the same commit as the decoder change.
5. Never add a test mode that silently rewrites expected values.

Generated PPM/PGM under `host/.out/` are developer artifacts, not goldens.
