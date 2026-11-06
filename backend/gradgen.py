#!/usr/bin/env python3
"""gradgen - generate gradient desktop backgrounds.

Layers: a base linear gradient at a random angle, several soft radial "glows"
from random directions, optional domain warping (makes it flow/swirl), and
fine grain to kill banding. Palettes are built from harmony rules in HSV space
so the results stay pleasant.

Examples:
  ./gradgen.py                         # 6 images, 1920x1200, into ./out
  ./gradgen.py -n 12 -s 4k
  ./gradgen.py -s iphone-15 -n 4
  ./gradgen.py -s 3000x2000            # custom size
  ./gradgen.py --seed 42 --style aurora
  ./gradgen.py --palette "#1a2a6c,#b21f1f,#fdbb2d"
  ./gradgen.py -s 4k -f webp -q 90     # smaller files for wallpapers
  ./gradgen.py --soft                  # calmer, lower-contrast palettes
  ./gradgen.py --palette-seed 7        # one shared palette for the whole batch
  ./gradgen.py --interp rgb            # legacy sRGB blending instead of OKLab
"""
import argparse, colorsys, math, os, random, time
import numpy as np
from PIL import Image

STYLES = ("linear", "glow", "aurora", "mesh", "any")

PRESETS = {
    # desktops / laptops
    "hd": (1920, 1080), "wuxga": (1920, 1200), "16x10-4k": (3840, 2400), "qhd": (2560, 1440), "4k": (3840, 2160), "5k": (5120, 2880),
    "macbook-air-13": (2560, 1664), "macbook-pro-14": (3024, 1964), "macbook-pro-16": (3456, 2234),
    "laptop-1610": (2560, 1600), "framework-13": (2256, 1504), "surface": (2736, 1824),
    # ultrawide
    "uw-1080": (2560, 1080), "uw-1440": (3440, 1440), "suw": (5120, 1440),
    # tablets
    "ipad": (2360, 1640), "ipad-pro-13": (2752, 2064),
    # phones (portrait)
    "phone-1080": (1080, 1920), "phone-tall": (1080, 2400), "iphone-15": (1179, 2556),
    "iphone-pro-max": (1290, 2796), "pixel-8": (1080, 2400), "galaxy-s24-ultra": (1440, 3120),
}


def hsv(h, s, v):
    return np.array(colorsys.hsv_to_rgb(h % 1.0, min(max(s, 0), 1), min(max(v, 0), 1)))


def hexc(s):
    s = s.strip().lstrip("#")
    if len(s) == 3:
        s = "".join(c * 2 for c in s)
    if len(s) != 6 or any(c not in "0123456789abcdefABCDEF" for c in s):
        raise ValueError(f"invalid hex color '{s}' (use #rgb or #rrggbb)")
    return np.array([int(s[i:i + 2], 16) / 255 for i in (0, 2, 4)])


# --- OKLab: perceptually uniform colour, so ramps don't go muddy mid-way ---

def srgb_to_linear(c):
    c = np.asarray(c, dtype=np.float32)
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)


def linear_to_srgb(c):
    c = np.clip(c, 0, 1)
    return np.where(c <= 0.0031308, c * 12.92, 1.055 * np.power(c, 1 / 2.4) - 0.055)


def rgb_to_oklab(rgb):
    r, g, b = (srgb_to_linear(rgb[..., i]) for i in range(3))
    l = 0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b
    m = 0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b
    s = 0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b
    l_, m_, s_ = np.cbrt(l), np.cbrt(m), np.cbrt(s)
    return np.stack([
        0.2104542553 * l_ + 0.7936177850 * m_ - 0.0040720468 * s_,
        1.9779984951 * l_ - 2.4285922050 * m_ + 0.4505937099 * s_,
        0.0259040371 * l_ + 0.7827717662 * m_ - 0.8086757660 * s_,
    ], axis=-1)


def oklab_to_rgb(lab):
    L, A, B = (lab[..., i] for i in range(3))
    l_ = L + 0.3963377774 * A + 0.2158037573 * B
    m_ = L - 0.1055613458 * A - 0.0638541728 * B
    s_ = L - 0.0894841775 * A - 1.2914855480 * B
    l, m, s = l_ ** 3, m_ ** 3, s_ ** 3
    r = 4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s
    g = -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s
    b = -0.0041960863 * l - 0.7034186147 * m + 1.7076147010 * s
    return linear_to_srgb(np.stack([r, g, b], axis=-1))


def make_palette(rng, n, soft=False):
    """Pick a harmony scheme and return n colors (float RGB 0-1).

    soft biases toward analogous/split-complement hues, tighter saturation and
    a gentler brightness ramp, so the result reads calm instead of punchy.
    """
    base = rng.random()
    if soft:
        scheme = rng.choice(["analogous", "analogous", "split", "mono", "split"])
    else:
        scheme = rng.choice(["analogous", "analogous", "split", "mono", "complement", "triad"])
    offs = {
        "analogous": [0, 0.06, -0.06, 0.12, -0.12],
        "complement": [0, 0.5, 0.04, 0.54, -0.04],
        "triad": [0, 1 / 3, 2 / 3, 0.05, 0.38],
        "split": [0, 0.42, 0.58, 0.05, -0.05],
        "mono": [0, 0.02, -0.02, 0.03, -0.03],
    }[scheme]
    jitter = 0.01 if soft else 0.02
    cols = []
    for i in range(n):
        h = base + offs[i % len(offs)] + rng.uniform(-jitter, jitter)
        s = rng.uniform(0.50, 0.78) if soft else rng.uniform(0.55, 0.95)
        # spread brightness across the ramp: one deep, one light, rest between
        ratio = i / max(n - 1, 1)
        if soft:
            v = 0.38 + 0.42 * ratio + rng.uniform(-0.04, 0.04)
        else:
            v = 0.18 + 0.8 * ratio + rng.uniform(-0.08, 0.08)
        cols.append(hsv(h, s, v))
    # order by brightness so ramps flow dark->light (avoids muddy mid-mixes)
    cols.sort(key=lambda c: c.sum())
    if rng.random() < 0.5:
        cols.reverse()
    return cols


def grid(w, h):
    # normalised coords, aspect-correct: x in [-a, a], y in [-1, 1]
    a = w / h
    y, x = np.mgrid[0:h, 0:w].astype(np.float32)
    return (x / (w - 1) * 2 - 1) * a, (y / (h - 1)) * 2 - 1


def smooth(t):
    return t * t * (3 - 2 * t)


def ramp(t, cols, interp="oklab"):
    """Map t in [0,1] through a multi-stop color ramp.

    Interpolates in OKLab by default so blends stay perceptually even instead
    of dipping through muddy, over-saturated midpoints the way sRGB does.
    """
    t = np.clip(t, 0, 1) * (len(cols) - 1)
    i = np.minimum(t.astype(int), len(cols) - 2)
    f = smooth(t - i)[..., None]
    arr = np.stack(cols)
    if interp == "oklab":
        arr = rgb_to_oklab(arr)
    out = arr[i] * (1 - f) + arr[i + 1] * f
    if interp == "oklab":
        out = oklab_to_rgb(out)
    return out


def warp_field(X, Y, rng, strength):
    """Cheap low-frequency domain warp from summed sines."""
    dx = np.zeros_like(X)
    dy = np.zeros_like(Y)
    for _ in range(3):
        fx, fy = rng.uniform(0.3, 1.0), rng.uniform(0.3, 1.0)
        p1, p2 = rng.uniform(0, math.tau), rng.uniform(0, math.tau)
        amp = rng.uniform(0.3, 1.0) * strength
        dx += amp * np.sin(fx * Y * math.pi + fy * X + p1)
        dy += amp * np.sin(fy * X * math.pi + fx * Y + p2)
    return X + dx, Y + dy


def render(w, h, rng, style, palette=None, grain=0.025, warp=None, soft=False,
           interp="oklab"):
    if style == "any":
        style = rng.choice(["linear", "glow", "aurora", "mesh"])
    cols = palette or make_palette(rng, rng.randint(3, 5), soft)
    X, Y = grid(w, h)

    if warp is None:
        warp = {"linear": 0.0, "glow": 0.0, "aurora": 0.12, "mesh": 0.15}[style]
    if warp:
        X, Y = warp_field(X, Y, rng, warp)

    # Base: linear gradient at a random angle
    ang = rng.uniform(0, math.tau)
    d = X * math.cos(ang) + Y * math.sin(ang)
    d = (d - d.min()) / (d.max() - d.min() + 1e-9)
    base_cols = cols if style != "mesh" else [cols[0], cols[1]]
    img = ramp(d, base_cols, interp)
    a = w / h

    def glow(cx, cy, rx, ry, col, power, strength):
        nonlocal img
        r = np.sqrt(((X - cx) / rx) ** 2 + ((Y - cy) / ry) ** 2)
        m = np.clip(1 - r, 0, 1) ** power * strength
        img = img * (1 - m[..., None]) + col * m[..., None]

    if style == "glow":
        for _ in range(rng.randint(2, 4)):
            glow(rng.uniform(-a, a), rng.uniform(-1, 1),
                 rng.uniform(0.8, 2.2), rng.uniform(0.8, 2.0),
                 rng.choice(cols), rng.uniform(1.0, 2.2), rng.uniform(0.6, 0.95))
    elif style == "mesh":
        # many big soft blobs ~ mesh gradient
        for c in cols + cols[:2]:
            glow(rng.uniform(-a * 1.1, a * 1.1), rng.uniform(-1.1, 1.1),
                 rng.uniform(1.0, 2.0), rng.uniform(1.0, 2.0), c,
                 rng.uniform(0.7, 1.4), rng.uniform(0.7, 1.0))
    elif style == "aurora":
        # dark base + bright elongated ribbons along the warped angle
        img = img * rng.uniform(0.25, 0.5)
        for _ in range(rng.randint(2, 4)):
            ang2 = rng.uniform(-0.6, 0.6) + (math.pi / 2 if rng.random() < 0.3 else 0)
            off = rng.uniform(-0.8, 0.8)
            width = rng.uniform(0.15, 0.45)
            dist = -X * math.sin(ang2) + Y * math.cos(ang2) - off
            band = np.exp(-(dist / width) ** 2)
            img = img + band[..., None] * rng.choice(cols) * rng.uniform(0.5, 0.9)
        img = np.clip(img, 0, 1)

    # soft vignette
    if rng.random() < 0.7:
        r = np.sqrt((X / a) ** 2 + Y ** 2)
        img = img * (1 - 0.25 * np.clip(r - 0.4, 0, 1)[..., None])

    # grain (also dithers 8-bit banding)
    if grain:
        img = img + np.random.default_rng(rng.randrange(2**32)).normal(0, grain, img.shape[:2])[..., None]

    return Image.fromarray((np.clip(img, 0, 1) * 255 + 0.5).astype(np.uint8)), style


def main():
    p = argparse.ArgumentParser(description="Generate gradient wallpapers.")
    p.add_argument("-n", "--count", type=int, default=6)
    p.add_argument("-s", "--size", default="wuxga",
                   help="WxH (e.g. 3000x2000) or preset name; see --list-sizes")
    p.add_argument("--list-sizes", action="store_true")
    p.add_argument("-o", "--out", default="out")
    p.add_argument("--style", choices=STYLES, default="any")
    p.add_argument("--seed", type=int, help="base seed (image i uses seed+i)")
    p.add_argument("--palette", help="comma-separated hex colors, e.g. '#102,#f80,#0cf'")
    p.add_argument("--grain", type=float, default=0.025)
    p.add_argument("--warp", type=float, help="override warp strength (0-1)")
    p.add_argument("-f", "--format", choices=("png", "webp", "jpg"),
                   default="png", help="output format (default: png)")
    p.add_argument("-q", "--quality", type=int, default=92,
                   help="quality for webp/jpg, 1-100 (default: 92)")
    p.add_argument("--soft", action="store_true",
                   help="calmer palettes: closer hues, lower saturation, gentler contrast")
    p.add_argument("--interp", choices=("oklab", "rgb"), default="oklab",
                   help="color interpolation space (default: oklab)")
    p.add_argument("--palette-seed", type=int,
                   help="generate one palette from this seed and reuse it for the whole batch")
    a = p.parse_args()

    if a.list_sizes:
        for k, (pw, ph) in PRESETS.items():
            print(f"{k:20s} {pw}x{ph}")
        return
    sz = a.size.lower()
    if sz in PRESETS:
        w, h = PRESETS[sz]
    else:
        try:
            w, h = map(int, sz.split("x"))
        except ValueError:
            p.error(f"unknown size '{a.size}' (use WxH or a preset; --list-sizes)")
    os.makedirs(a.out, exist_ok=True)
    if a.palette:
        try:
            pal = [hexc(c) for c in a.palette.split(",")]
        except ValueError as e:
            p.error(str(e))
        if len(pal) < 2:
            p.error("--palette needs at least 2 colors")
    else:
        pal = None
    # Optional cohesive batch: one palette for every image, chosen from its own
    # RNG so --seed still controls composition independently.
    if pal is None and a.palette_seed is not None:
        prng = random.Random(a.palette_seed)
        pal = make_palette(prng, prng.randint(3, 5), a.soft)
    ext = "jpg" if a.format in ("jpg", "jpeg") else a.format
    base = a.seed if a.seed is not None else time.time_ns() % (2 ** 32)
    ptag = f"p{a.palette_seed}_" if a.palette_seed is not None else ""
    for i in range(a.count):
        seed = base + i
        img, style = render(w, h, random.Random(seed), a.style, pal, a.grain,
                            a.warp, a.soft, a.interp)
        path = os.path.join(a.out, f"grad_{'soft_' if a.soft else ''}{style}_{ptag}{seed}_{w}x{h}.{ext}")
        if ext == "png":
            img.save(path, optimize=True)
        elif ext == "webp":
            img.save(path, quality=max(1, min(100, a.quality)), method=6)
        else:
            img.convert("RGB").save(path, quality=max(1, min(100, a.quality)), optimize=True)
        print(path)


if __name__ == "__main__":
    main()
