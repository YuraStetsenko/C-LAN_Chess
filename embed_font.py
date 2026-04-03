from pathlib import Path
import re

INPUT_DIR = Path("Chess")
OUTPUT_FILE = Path("Chess/font_res.hpp")

ALLOWED_EXTS = {".ttf"}

def sanitize(name: str) -> str:
    name = name.lower()
    name = re.sub(r'[^a-z0-9_]', '_', name)
    if name and name[0].isdigit():
        name = "_" + name
    return name

files = sorted(
    [p for p in INPUT_DIR.iterdir() if p.is_file() and p.suffix.lower() in ALLOWED_EXTS]
)

with OUTPUT_FILE.open("w", encoding="utf-8") as out:
    out.write("#pragma once\n")
    out.write("#include <cstddef>\n\n")
    out.write("namespace resource {\n\n")
    out.write(f"constexpr unsigned char * resources[] = {{\n    ")

    for file in files:
        var_name = sanitize(file.stem + "_" + file.suffix[1:])
        out.write(f"{var_name},\n")

    out.write(f"}}\n\n")

    for file in files:
        var_name = sanitize(file.stem + "_" + file.suffix[1:])
        data = file.read_bytes()

        out.write(f"inline constexpr unsigned char {var_name}[] = {{\n")

        for i, b in enumerate(data):
            if i % 12 == 0:
                out.write("    ")
            out.write(f"0x{b:02X}")
            if i != len(data) - 1:
                out.write(", ")
            if i % 12 == 11:
                out.write("\n")

        if len(data) % 12 != 0:
            out.write("\n")

        out.write(f"}}; \n\n")


    out.write("} // namespace resource\n")