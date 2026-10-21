#!/usr/bin/env python3
"""Screenshot comparison: reference vs current.
Usage: compare.py REFERENCE.png CURRENT.png OUTDIR [--threshold N]
Refuses to compare images of different size (compare like with like).
Writes overlay.png (50/50), diff.png (amplified), report.json/.txt.
"""
import sys, json, os
import numpy as np
from PIL import Image
from scipy import ndimage  # for region bounding boxes

def main():
    a = sys.argv[1:]
    if len(a) < 3: sys.exit(__doc__)
    ref_p, cur_p, out = a[0], a[1], a[2]
    thr = int(a[a.index('--threshold')+1]) if '--threshold' in a else 8
    ref = Image.open(ref_p).convert('RGB'); cur = Image.open(cur_p).convert('RGB')
    if ref.size != cur.size:
        sys.exit(f"ERROR: size mismatch ref={ref.size} cur={cur.size}. "
                 "Capture at identical window size/scale before comparing.")
    os.makedirs(out, exist_ok=True)
    R = np.asarray(ref, dtype=np.int16); C = np.asarray(cur, dtype=np.int16)
    d = np.abs(R - C)
    per_px = d.max(axis=2)
    mask = per_px > thr
    Image.blend(ref, cur, 0.5).save(f"{out}/overlay.png")
    Image.fromarray(np.clip(d * 4, 0, 255).astype(np.uint8)).save(f"{out}/diff.png")
    lab, n = ndimage.label(ndimage.binary_dilation(mask, iterations=3))
    boxes = []
    for i, sl in enumerate(ndimage.find_objects(lab)):
        area = int(mask[sl].sum())
        boxes.append({"x": sl[1].start, "y": sl[0].start, "w": sl[1].stop - sl[1].start,
                      "h": sl[0].stop - sl[0].start, "differing_px": area})
    boxes.sort(key=lambda b: -b["differing_px"])
    rep = {"size": list(ref.size), "threshold": thr,
           "differing_pixels": int(mask.sum()),
           "differing_pct": round(100.0 * mask.mean(), 4),
           "mean_abs_error": round(float(d.mean()), 4),
           "top_regions": boxes[:10]}
    json.dump(rep, open(f"{out}/report.json", "w"), indent=2)
    print(json.dumps(rep, indent=2))

if __name__ == "__main__":
    main()
