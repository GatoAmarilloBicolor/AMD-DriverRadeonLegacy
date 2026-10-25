#!/usr/bin/env python3
"""Genera src/server/asic/rg_asic_db.inc (C8 / §7A.3).

Fuentes, si estan disponibles:
  --mesa-dir    raiz de Mesa (include/pci_ids/*.h, src/amd/common/amd_family.h)
  --libdrm-dir  raiz de libdrm (data/amdgpu.ids)

Sin fuentes usa la tabla semilla embebida. El test de cobertura
(tests/test_main.cpp, G0) falla si algun ID queda sin familia.
"""
import argparse
import re
import sys
from pathlib import Path

FAMILY_RE = {
    "r300": "RG_FAMILY_R300",
    "r600": "RG_FAMILY_R600",
    "evergreen": "RG_FAMILY_EVERGREEN",
    "northern_islands": "RG_FAMILY_NI",
    "ni": "RG_FAMILY_NI",
    "gcn1": "RG_FAMILY_GCN1",
    "gcn2": "RG_FAMILY_GCN2",
    "gcn3": "RG_FAMILY_GCN3",
    "gcn5": "RG_FAMILY_GCN5",
    "rdna1": "RG_FAMILY_RDNA1",
    "rdna2": "RG_FAMILY_RDNA2",
    "rdna3": "RG_FAMILY_RDNA3",
    "rdna4": "RG_FAMILY_RDNA4",
}

# (nombre, device, revision, submask, familia, gfx, apu, vram, features)
SEED = [
    # R300 / R400 / R500
    ("Radeon 9500 (R300)",        0x4144, 0, 0xFFFFFFFF, "R300",       0, 0, "DDR",   0),
    ("Radeon 9800 (R350)",        0x4E48, 0, 0xFFFFFFFF, "R300",       0, 0, "DDR",   0),
    ("Radeon X1600 (RV530)",      0x71C0, 0, 0xFFFFFFFF, "R300",       0, 0, "DDR2",  0),
    ("Radeon X1950 (RV570)",      0x7280, 0, 0xFFFFFFFF, "R300",       0, 0, "GDDR3", 0),
    
    # R600 / R700 (HD 2000 - HD 4000 series)
    ("Radeon HD 2900 (R600)",     0x9400, 0, 0xFFFFFFFF, "R600",       0, 0, "GDDR3", 0),
    ("Radeon HD 2600 (RV630)",    0x9587, 0, 0xFFFFFFFF, "R600",       0, 0, "DDR2",  0),
    ("Radeon HD 3650 (RV635)",    0x9598, 0, 0xFFFFFFFF, "R600",       0, 0, "DDR3",  0),
    ("Radeon HD 3870 (RV670)",    0x9501, 0, 0xFFFFFFFF, "R600",       0, 0, "GDDR4", 0),
    ("Radeon HD 4850 (RV770)",    0x9442, 0, 0xFFFFFFFF, "R600",       0, 0, "GDDR3", 0),
    ("Radeon HD 4870 (RV770)",    0x9440, 0, 0xFFFFFFFF, "R600",       0, 0, "GDDR5", 0),
    ("Radeon HD 4770 (RV740)",    0x94B4, 0, 0xFFFFFFFF, "R600",       0, 0, "GDDR5", 0),
    ("Radeon HD 4350 (RV710)",    0x954F, 0, 0xFFFFFFFF, "R600",       0, 0, "DDR2",  0),

    # Evergreen (HD 5000 / HD 6000 series)
    ("Radeon HD 5450 (Cedar)",    0x68F9, 0, 0xFFFFFFFF, "EVERGREEN",  0, 0, "DDR3",  0),
    ("Radeon HD 5570 (Redwood)",  0x68D8, 0, 0xFFFFFFFF, "EVERGREEN",  0, 0, "GDDR5", 0),
    ("Radeon HD 5770 (Juniper)",  0x68B8, 0, 0xFFFFFFFF, "EVERGREEN",  0, 0, "GDDR5", 0),
    ("Radeon HD 5870 (Cypress)",  0x6898, 0, 0xFFFFFFFF, "EVERGREEN",  0, 0, "GDDR5", 0),
    ("Radeon HD 6870 (Barts)",    0x6738, 0, 0xFFFFFFFF, "EVERGREEN",  0, 0, "GDDR5", 0),
    ("Radeon HD 6770 (Juniper)",  0x68BA, 0, 0xFFFFFFFF, "EVERGREEN",  0, 0, "GDDR5", 0),

    # APUs Bobcat / Ontario (AMD C-70 / Radeon HD 6290) y Kabini / Temash (HD 8250 / 8750M)
    ("Radeon HD 6290 (Ontario/C-70)", 0x9806, 0, 0xFFFFFFFF, "EVERGREEN",  0, 1, "DDR",   0),
    ("Radeon HD 6310 (Wrestler)",     0x9802, 0, 0xFFFFFFFF, "EVERGREEN",  0, 1, "DDR",   0),
    ("Radeon HD 8250 (Temash)",       0x9830, 0, 0xFFFFFFFF, "NI",         0, 1, "GDDR5", 0),
    ("Radeon HD 8750M (Oland)",       0x6640, 0, 0xFFFFFFFF, "NI",         0, 0, "GDDR5", 0),

    # GCN & Modern
    ("Radeon HD 7970 (Tahiti)",   0x6798, 0, 0xFFFFFFFF, "GCN1",       6, 0, "GDDR5", 0x03),
    ("Radeon R9 290 (Hawaii)",    0x67B0, 0, 0xFFFFFFFF, "GCN2",       7, 0, "GDDR5", 0x07),
    ("Radeon RX 480 (Ellesmere)", 0x67DF, 0xC7, 0xFFFFFFFF, "GCN3",    8, 0, "GDDR5", 0x1F),
    ("Radeon RX 580 (Polaris20)", 0x67DF, 0xE7, 0xFFFFFFFF, "GCN3",    8, 0, "GDDR5", 0x1F),
    ("Radeon Vega 64 (Vega10)",   0x687F, 0, 0xFFFFFFFF, "GCN5",       9, 0, "HBM2",  0x1F),
    ("Radeon RX 5700 (Navi10)",   0x731F, 0, 0xFFFFFFFF, "RDNA1",     10, 0, "GDDR6", 0x1F),
    ("Radeon RX 6800 (Sienna)",   0x73BF, 0, 0xFFFFFFFF, "RDNA2",     10, 0, "GDDR6", 0x1F),
    ("Radeon RX 7900 (Navi31)",   0x744C, 0, 0xFFFFFFFF, "RDNA3",     11, 0, "GDDR6", 0x1F),
]

FEATURE_BITS = {
    "vm": "RG_ASIC_FEAT_VM",
    "sdma": "RG_ASIC_FEAT_SDMA",
    "mec": "RG_ASIC_FEAT_MEC",
    "psp": "RG_ASIC_FEAT_PSP",
    "smu": "RG_ASIC_FEAT_SMU",
}


def emit(entries):
    out = []
    out.append("/*")
    out.append(" * rg_asic_db.inc - Base de datos generada por scripts/gen_asic_db.py.")
    out.append(" * NO EDITAR A MANO: regenerar con `make asic-db`.")
    out.append(" */")
    for name, dev, rev, sub, fam, gfx, apu, vram, feat in entries:
        fam_c = FAMILY_RE.get(fam.lower(), "RG_FAMILY_UNKNOWN")
        feat_c = "0" if not feat else "|".join(
            v for k, v in FEATURE_BITS.items() if feat & (1 << list(FEATURE_BITS).index(k)))
        out.append(
            f'RG_ASIC_ENTRY("{name}", 0x{dev:04X}, 0x{rev:02X}, 0x{sub:08X}, '
            f'{fam_c}, {gfx}, {apu}, RG_VRAM_{vram}, {feat_c}),')
    out.append("")
    return "\n".join(out)


def emit_pci_header(entries):
    """Lista unica de device IDs para el helper de kernel (sin duplicados)."""
    ids = sorted({e[1] for e in entries})
    out = [
        "/*",
        " * rg_pci_ids.h - IDs PCI de las GPU que el driver reconoce.",
        " *",
        " * GENERADO por scripts/gen_asic_db.py junto con rg_asic_db.inc.",
        " * No editar a mano: ejecutar `make asic-db`.",
        " */",
        "#ifndef RG_PCI_IDS_H",
        "#define RG_PCI_IDS_H",
        "",
        "#include <stdint.h>",
        "",
        "#define RG_PCI_VENDOR_AMD 0x1002",
        "",
        "#define RG_PCI_DEVICE_LIST(X) \\",
    ]
    for i, dev in enumerate(ids):
        sep = " \\" if i + 1 < len(ids) else ""
        out.append(f"\tX(0x{dev:04X}){sep}")
    out += [
        "",
        "#define RG_PCI_ID_CASE(id) case id:",
        "",
        "static inline int rg_pci_id_is_supported(uint32_t device_id)",
        "{",
        "\tswitch (device_id) {",
        "\t\tRG_PCI_DEVICE_LIST(RG_PCI_ID_CASE)",
        "\t\t\treturn 1;",
        "\t\tdefault:",
        "\t\t\treturn 0;",
        "\t}",
        "}",
        "",
        "#endif /* RG_PCI_IDS_H */",
        "",
    ]
    return "\n".join(out)


def parse_mesa_ids(mesa_dir):
    """Extrae CHIPSET(...) de include/pci_ids/*.h si existen."""
    entries = []
    pci = Path(mesa_dir) / "include" / "pci_ids"
    if not pci.is_dir():
        return entries
    pattern = re.compile(
        r"CHIPSET\(\s*([0-9A-Fa-fx]+)\s*,\s*[^,]+,\s*([A-Za-z0-9_]+)\s*\)")
    for header in sorted(pci.glob("*.h")):
        fam = header.stem
        if fam.startswith("radeon") or fam.startswith("r600") or fam.startswith("r300"):
            family = "R600"
        elif fam.startswith("amdgpu"):
            family = "GCN3"
        else:
            continue
        for m in pattern.finditer(header.read_text(errors="ignore")):
            dev = int(m.group(1), 0)
            name = m.group(2)
            entries.append((name, dev, 0, 0xFFFFFFFF, family, 0, 0, "GDDR5", 0))
    return entries


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--mesa-dir")
    ap.add_argument("--libdrm-dir")
    ap.add_argument("--output", default="src/server/asic/rg_asic_db.inc")
    ap.add_argument("--pci-header", default="src/common/rg_pci_ids.h")
    args = ap.parse_args()

    entries = parse_mesa_ids(args.mesa_dir) if args.mesa_dir else []
    if not entries:
        if args.mesa_dir:
            print("warning: sin IDs en Mesa; usando tabla semilla", file=sys.stderr)
        entries = SEED
    else:
        # completar con la semilla para no perder cobertura legacy
        known = {e[1] for e in entries}
        entries += [e for e in SEED if e[1] not in known]

    text = emit(entries)
    Path(args.output).parent.mkdir(parents=True, exist_ok=True)
    Path(args.output).write_text(text)
    print(f"gen_asic_db: {len(entries)} entradas -> {args.output}")

    if args.pci_header:
        Path(args.pci_header).parent.mkdir(parents=True, exist_ok=True)
        Path(args.pci_header).write_text(emit_pci_header(entries))
        print(f"gen_asic_db: IDs PCI -> {args.pci_header}")

    unknown = [e for e in entries if e[4].lower() not in FAMILY_RE]
    if unknown:
        print(f"error: {len(unknown)} IDs sin familia", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
