#!/usr/bin/env python3
"""Build MuOS assignment metadata from FrogUI's core folder table."""

import json
import re
import shutil
import sys
from pathlib import Path

FAMILIES = {
    "fceumm": ("Nintendo NES-Famicom", ("fceumm", "quicknes", "nestopia")),
    "quicknes": ("Nintendo NES-Famicom", ("fceumm", "quicknes", "nestopia")),
    "nestopia": ("Nintendo NES-Famicom", ("fceumm", "quicknes", "nestopia")),
    "snes9x2005_plus": ("Nintendo SNES-SFC", ("snes9x2005_plus", "snes9x2002", "snes9x2010", "snes9x-Z")),
    "snes9x2002": ("Nintendo SNES-SFC", ("snes9x2005_plus", "snes9x2002", "snes9x2010", "snes9x-Z")),
    "snes9x2010": ("Nintendo SNES-SFC", ("snes9x2005_plus", "snes9x2002", "snes9x2010", "snes9x-Z")),
    "snes9x-Z": ("Nintendo SNES-SFC", ("snes9x2005_plus", "snes9x2002", "snes9x2010", "snes9x-Z")),
    "gambatte": ("Nintendo Game Boy", ("gambatte", "gearboy", "tgbdual")),
    "gearboy": ("Nintendo Game Boy", ("gambatte", "gearboy", "tgbdual")),
    "tgbdual": ("Nintendo Game Boy", ("gambatte", "gearboy", "tgbdual")),
    "gpsp": ("Nintendo Game Boy Advance", ("gpsp", "gpsp_multicore", "mgba", "vba_next")),
    "gpsp_multicore": ("Nintendo Game Boy Advance", ("gpsp", "gpsp_multicore", "mgba", "vba_next")),
    "mgba": ("Nintendo Game Boy Advance", ("gpsp", "gpsp_multicore", "mgba", "vba_next")),
    "vba_next": ("Nintendo Game Boy Advance", ("gpsp", "gpsp_multicore", "mgba", "vba_next")),
    "picodrive": ("Sega Mega Drive - Genesis", ("picodrive", "genesis_plus_gx")),
    "genesis_plus_gx": ("Sega Mega Drive - Genesis", ("picodrive", "genesis_plus_gx")),
    "gearsystem": ("Sega Master System", ("gearsystem", "picodrive")),
    "pcsx_rearmed": ("Sony Playstation", ("pcsx_rearmed", "qpsx")),
    "qpsx": ("Sony Playstation", ("pcsx_rearmed", "qpsx")),
    "mame2000": ("Arcade", ("mame2000", "mame2003_plus", "fbneo")),
    "mame2003_plus": ("Arcade", ("mame2000", "mame2003_plus", "fbneo")),
    "fbneo": ("Arcade", ("mame2000", "mame2003_plus", "fbneo")),
}

NAMES = {
    "fceumm": "FCEUmm", "quicknes": "QuickNES", "nestopia": "Nestopia",
    "snes9x2005_plus": "Snes9x 2005 Plus", "snes9x2002": "Snes9x 2002",
    "snes9x2010": "Snes9x 2010", "snes9x-Z": "Snes9x Z", "gambatte": "Gambatte",
    "gearboy": "Gearboy", "tgbdual": "TGB Dual", "gpsp": "gpSP",
    "gpsp_multicore": "gpSP Multicore", "mgba": "mGBA", "vba_next": "VBA Next",
    "picodrive": "PicoDrive", "genesis_plus_gx": "Genesis Plus GX", "gearsystem": "Gearsystem",
    "pcsx_rearmed": "PCSX ReARMed", "qpsx": "QPSX", "mame2000": "MAME 2000",
    "mame2003_plus": "MAME 2003 Plus", "fbneo": "FinalBurn Neo",
}

PATTERN = re.compile(r'^\s*\{"([^\"]+)",\s*CORES_PATH\s*"/([^\"]+)_libretro\.so"\}')


def main():
    if len(sys.argv) != 4:
        raise SystemExit("usage: generate_frogui_coredb.py FROGUI_LIBRETRO_C CORES_DIR OUTPUT_DIR")
    source, cores_dir, output = map(Path, sys.argv[1:])
    cores_dir, output = cores_dir.resolve(), output.resolve()
    if not source.is_file() or not cores_dir.is_dir():
        raise SystemExit("FrogUI mapping source or installed core directory is missing")
    if output.exists():
        shutil.rmtree(output)
    core_dir = output / "core" / "treefrogui"
    core_dir.mkdir(parents=True)

    assignments = {}
    systems = {}
    for line in source.read_text().splitlines():
        match = PATTERN.match(line)
        if not match:
            continue
        folder, core = match.groups()
        key = re.sub(r"[ _+-]", "", folder.lower())
        core_file = f"{core}_libretro.so"
        if not key or key in assignments or not (cores_dir / core_file).is_file():
            continue

        system = f"frogui_{key}"
        assignments[key] = system
        catalogue, candidates = FAMILIES.get(core, (folder.upper(), (core,)))
        available = [candidate for candidate in candidates if (cores_dir / f"{candidate}_libretro.so").is_file()]
        if core not in available:
            available.insert(0, core)
        definitions = {}
        for candidate in dict.fromkeys(available):
            definitions[candidate] = {
                "name": NAMES.get(candidate, candidate.replace("_", " ").title()),
                "core": f"{candidate}_libretro.so",
            }
        systems[system] = {
            "catalogue": catalogue,
            "runtime": "retroarch",
            "default": core,
            "libretro": definitions,
        }

    if not assignments:
        raise SystemExit("no FrogUI libretro folder mappings matched installed cores")
    (output / "assign.json").write_text(json.dumps(assignments, indent=2) + "\n")
    for system, definition in systems.items():
        (core_dir / f"{system}.json").write_text(json.dumps(definition, separators=(",", ":")) + "\n")
    print(f"Generated {len(assignments)} FrogUI folder assignments and {len(systems)} MuOS core definitions")


if __name__ == "__main__":
    main()
