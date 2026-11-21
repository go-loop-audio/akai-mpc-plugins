import sys, numpy as np, glob, os
a, b = sys.argv[1], sys.argv[2]
tol = float(sys.argv[3]) if len(sys.argv) > 3 else 0.0
only = sys.argv[4] if len(sys.argv) > 4 else None
bad = 0
for fa in sorted(glob.glob(os.path.join(a, '*.f32'))):
    fb = os.path.join(b, os.path.basename(fa))
    x = np.fromfile(fa, dtype='<f4'); y = np.fromfile(fb, dtype='<f4')
    name = os.path.basename(fa)[:-4]
    if only and name != only: continue
    if x.shape != y.shape:
        print(f'FAIL {name}: length {x.shape} vs {y.shape}'); bad += 1; continue
    diff = int(np.count_nonzero(x != y))
    peak = float(np.max(np.abs(x))) if x.size else 0.0
    mx = float(np.max(np.abs(x - y))) if x.size else 0.0
    ok = mx <= tol and peak > 1e-3 and np.isfinite(x).all() and np.isfinite(y).all()
    print(f'{"PASS" if ok else "FAIL"} {name:24s} peak={peak:.4f} differing samples={diff} max|diff|={mx:.3g}')
    bad += 0 if ok else 1
sys.exit(1 if bad else 0)
