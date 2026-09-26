from pathlib import Path
import re

p = list(Path(r"c:\Projects\esp2\3dprint\reference\waveshare").rglob("*.stp"))[0]
print("STEP", p, p.stat().st_size)
text = p.read_text(errors="ignore")
pts = re.findall(
    r"CARTESIAN_POINT\s*\([^,]*,\s*\(\s*([-+eE0-9.]+)\s*,\s*([-+eE0-9.]+)\s*,\s*([-+eE0-9.]+)\s*\)",
    text,
)
print("points", len(pts))
xs = [float(a) for a, b, c in pts]
ys = [float(b) for a, b, c in pts]
zs = [float(c) for a, b, c in pts]
print("X", min(xs), max(xs), "span_mm", max(xs) - min(xs))
print("Y", min(ys), max(ys), "span_mm", max(ys) - min(ys))
print("Z", min(zs), max(zs), "span_mm", max(zs) - min(zs))
print("Z unique sample", sorted({round(z, 2) for z in zs})[:50])
