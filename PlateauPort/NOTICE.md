# Notice

PlateauPort (product name **GLA Plateau**, by Go Loop Audio) is based on **Plateau** from **ValleyRackFree** by Dale Johnson
(Valley Audio Soft): https://github.com/ValleyAudio/ValleyRackFree

Plateau implements the plate reverb from Jon Dattorro, "Effect Design Part 1:
Reverberator and Other Filters", J. Audio Eng. Soc. 45(9), 660-684 (1997).

- `src/Plateau/`, `src/dsp/`, `src/utilities/` are copied from ValleyRackFree.
  They are unchanged except for the one patch listed in PATCHES.md.
  `upstream-ref/` is a copy used by the tests; it is untouched except that the VCV Rack panel widget part of `Plateau.cpp.txt` was removed.
- `src/engine/` reproduces the parameter handling of ValleyRackFree's
  `Plateau::getParameters()` / `Plateau::process()` without the VCV Rack parts.
- Original copyright and licence headers are kept in every copied file.

## Licence

The ValleyRackFree README states: "All code is licensed under GNU Public License
v3.0, Copyright (c) 2023 Dale Johnson", so this project is distributed under
GPL-3.0 (see `LICENSE`). If you ship binaries (for example through the Open MPC
catalog) the corresponding source has to be available to recipients.

Two things to check before you publish, because they were not in the files used
to make this port:

1. The header of upstream `Plateau.hpp` carries a BSD-3-clause style notice.
   The repository also contains `LICENSE.md` and `LICENSE-dist.md`, which were not
   included in the source zip. Read them and make sure the terms agree with GPL-3.0.
2. "Plateau" and "Valley" are Valley Audio Soft's names. The product is named "GLA Plateau" and credits the
   original in its description; keep that credit, and check that the name is distinct enough
   for you if you release it publicly.

This is not legal advice.

## Akai MPC Plugin SDK

The plugin is built with the MPC Plugin SDK (inMusic / Akai Professional, MIT licence). Its header-only
framework (`framework/mpc_plugin.h` and friends) is compiled into the plugin binary, so keep the SDK's
`LICENSE` with any binary you distribute. The SDK is not copied into this repository.

## Artwork

The skin's knobs and background (`assets/`, `skins/`) are original artwork drawn by `tools/make_skin.py`. They use
only the colour scheme of the Valley Plateau panel (white, green, blue, red); no Valley graphics are included.
The branding text is set in Inter Medium (SIL Open Font License 1.1) and converted to images, so no font is shipped.
