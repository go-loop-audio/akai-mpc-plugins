#!/usr/bin/env python3
"""make_skin.py - build the hand-written MPC skin in skins/ for GLA Plateau.

Pipeline
  1. render the Rogan knob film strips (artwork from ValleyRackFree, skin-art/rogan/*.svg) and the
     page backgrounds into assets/;
  2. run the SDK's gen_ui.py on ui.json to get a TUI.json / Q-Links.json;
  3. patch the result: knobs get Valley's colour coding (white = levels, green = filters, blue =
     space, red = modulation) and (optionally) momentary buttons;
  4. write everything into skins/, which build.py then copies verbatim (it skips gen_ui.py when
     skins/ exists).

Dev tool: needs Python 3 with Pillow, rsvg-convert, the SDK checkout and build/params.json (written
by `make sdk-test`). The result in skins/ and assets/ is committed, so a normal build needs none of this.

  python3 tools/make_skin.py --sdk /path/to/mpc-plugin-builder-sdk-main
"""
import argparse, copy, json, os, shutil, subprocess, sys, tempfile
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
FRAMES, KSIZE, SS = 127, 90, 4          # frames, knob size in px, supersampling for rotation
SWEEP = 300.0                           # degrees from minimum to maximum
FONT = "/usr/share/fonts/opentype/inter/Inter-Medium.otf"   # SIL OFL 1.1; only used to draw backgrounds
BG = (0x28, 0x28, 0x28, 255)            # Valley's panel grey
WHITE, MUTED = (255, 255, 255, 255), (0x9a, 0x9a, 0x9c, 255)

# Valley's colour coding of the Plateau panel
KNOB_COLOUR = {
    "Dry": "white", "Wet": "white", "Pre-delay": "white",
    "In Low Cut": "green", "In High Cut": "green", "Rev Low Cut": "green", "Rev High Cut": "green",
    "Size": "blue", "Diffusion": "blue", "Decay": "blue",
    "Mod Rate": "red", "Mod Shape": "red", "Mod Depth": "red",
}
MOMENTARY = ()                        # names of buttons to spring back when released, e.g. ("Clear",); see README, untested/not working


def render_knob(colour, out):
    svg = os.path.join(ROOT, "skin-art", "rogan", "Rogan1PS%sMed.svg" % colour.capitalize())
    big = KSIZE * SS
    png = tempfile.mktemp(suffix=".png")
    subprocess.check_call(["rsvg-convert", "-w", str(big), "-h", str(big), svg, "-o", png])
    base = Image.open(png).convert("RGBA"); os.remove(png)
    strip = Image.new("RGBA", (KSIZE, KSIZE * FRAMES), (0, 0, 0, 0))
    for i in range(FRAMES):
        theta = -SWEEP / 2 + SWEEP * i / (FRAMES - 1)          # clockwise degrees from straight up
        fr = base.rotate(-theta, resample=Image.BICUBIC).resize((KSIZE, KSIZE), Image.LANCZOS)
        strip.paste(fr, (0, i * KSIZE))
    strip.save(out, optimize=True)


def render_background(out):
    # Branding sits in the empty lower part of the screen: the top 35 px belong to the first Q-Link
    # frame, which runs from y=35 to 313 (rows 1-2), so nothing may be drawn across that area's edges.
    img = Image.new("RGBA", (1280, 628), BG)
    d = ImageDraw.Draw(img)
    d.line([(23, 470), (1257, 470)], fill=WHITE, width=1)
    d.text((23, 488), "GLA Plateau", font=ImageFont.truetype(FONT, 52), fill=WHITE)
    d.text((23, 560), "Go Loop Audio", font=ImageFont.truetype(FONT, 32), fill=MUTED)
    img.save(out, optimize=True)


# ---- JSON helpers: gen_ui's Obj is an ordered list of pairs, so duplicate keys survive ----
def load(gen, path):
    return json.load(open(path), object_pairs_hook=lambda p: gen.Obj(p))

def get(o, key):
    for k, v in o:
        if k == key:
            return v
    raise KeyError(key)

def put(o, key, val):
    for i, (k, _) in enumerate(o):
        if k == key:
            o[i] = (key, val); return
    raise KeyError(key)

def walk(v):
    yield v
    if isinstance(v, list):
        for x in (v if not v or not isinstance(v[0], tuple) else [e for _, e in v]):
            yield from walk(x)


def patch_tui(gen, tui):
    page = get(tui, "pageData")
    defs = get(get(page, "componentDefinitions"), "localComponentDefinitions")
    by_key = {get(d, "key"): d for d in defs}

    def clone_knob(base_key, new_key, image):
        d = copy.deepcopy(by_key[base_key])
        put(d, "key", new_key)
        for n in walk(d):
            if isinstance(n, gen.Obj) and ("filmStrip", "knob_uni.png") in n or \
               isinstance(n, gen.Obj) and any(k == "filmStrip" for k, _ in n):
                put(n, "filmStrip", image)
        # gen_ui assumes a custom knobImage is 77 px and sizes the entry to match; ours are 90 px
        # (the kit's size), so give the knob its full 90 px and move the labels the way the kit layout does.
        want = {"knob": "8 8 90 90", "name": "110 12 156 36", "value": "110 52 156 42"}
        for ch in get(get(d, "value"), "componentsData"):
            cname = get(get(ch, "componentData"), "name")
            if cname in want:
                put(get(ch, "bounds"), "bounds", want[cname])
        defs.append(d); by_key[new_key] = d

    for colour in ("white", "green", "blue", "red"):
        clone_knob("knobUni", "knob_" + colour, "knob_%s.png" % colour)
    if "knobBi" in by_key:
        for colour in ("red",):
            clone_knob("knobBi", "knobBi_" + colour, "knob_%s.png" % colour)

    for tab in get(page, "tabs"):
        for child in get(get(tab, "componentDefinition"), "componentsData"):
            cd = get(child, "componentData")
            name, typ = get(cd, "name"), get(cd, "type")
            colour = KNOB_COLOUR.get(name)
            if colour and typ == "knobUni":
                put(cd, "type", "knob_" + colour)
            elif colour and typ == "knobBi":
                put(cd, "type", "knobBi_" + colour)
            if name in MOMENTARY:
                rm = get(get(child, "handle remapping"), "map")
                for i, pair in enumerate(rm):
                    if get(pair, "key") == "Data":
                        put(pair, "value", "ResetOnGestureEnd/" + get(pair, "value"))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sdk", default=os.path.join(ROOT, "..", "mpc-plugin-builder-sdk-main"))
    ap.add_argument("--params", default=os.path.join(ROOT, "build", "params.json"))
    a = ap.parse_args()
    sys.path.insert(0, os.path.join(a.sdk, "tools"))
    import gen_ui as gen

    assets = os.path.join(ROOT, "assets")
    os.makedirs(assets, exist_ok=True)
    for c in ("white", "green", "blue", "red"):
        render_knob(c, os.path.join(assets, "knob_%s.png" % c))
    render_background(os.path.join(assets, "bg_plateau.png"))

    tmp = tempfile.mkdtemp()
    gen.generate(a.params, os.path.join(ROOT, "ui.json"), tmp, assets, False, os.path.join(ROOT, "build"))
    tui = load(gen, os.path.join(tmp, "TUI.json"))
    patch_tui(gen, tui)
    open(os.path.join(tmp, "TUI.json"), "w").write(gen.dump_json(tui) + "\n")

    out = os.path.join(ROOT, "skins")
    shutil.rmtree(out, ignore_errors=True)
    shutil.copytree(tmp, out)
    for f in os.listdir(assets):               # knob strips + background (only the ones the TUI names)
        text = open(os.path.join(out, "TUI.json")).read()
        if f in text and not os.path.exists(os.path.join(out, f)):
            shutil.copy(os.path.join(assets, f), out)
    print("skins/ written:", sorted(os.listdir(out)))

if __name__ == "__main__":
    main()
