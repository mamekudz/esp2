# Framebuffer strategy

Physical panel: **280 × 456** RGB565 AMOLED (CO5300).
Apple II HGR: **280 × 192** logical.

## Options

| Strategy | RAM | Bandwidth | Fit |
| --- | --- | --- | --- |
| A. Full RGB565 panel FB | ~255 KiB | High if rewritten every frame | Possible in PSRAM; costly |
| B. Apple RGB565 280×192 + scale blit | ~105 KiB + scale | Medium | Good for early video |
| C. Line / strip buffers | 1–8 lines | Lowest | Preferred long-term |
| D. Indexed intermediate (palette) | Small | Needs expand to RGB565 | Useful for mono modes |
| E. Direct scanline to QSPI | Minimal | Depends on CO5300 path | Best if API allows |

## Recommendation (ESTIMATE)

1. **Host / early ESP:** Strategy B — render Apple II to 280×192 RGB565 in
   PSRAM, then viewport-scale into visible rectangle without rotating full
   buffers blindly (`CLAUDE.md` §13).
2. **Production target:** Strategy C/E hybrid — decode+color+effect one
   scanline (or small strip), push to display, avoid second full panel FB.
3. Keep Control Screen on a separate lightweight UI buffer or dirty-rect path.

Do **not** replace the working CO5300 bring-up path until a measured
prototype beats it.

## Rotation / enclosure

Portrait enclosure mode must use viewport transforms, not full framebuffer
rotation, when possible (CO5300 hardware rotation limits).
