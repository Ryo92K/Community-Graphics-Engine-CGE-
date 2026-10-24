                                                                                       

import hashlib

import json

from pathlib import Path

import struct

import sys

dll, project = Path(sys.argv[1]), Path(sys.argv[2])

data = dll.read_bytes()

u16 = lambda o: struct.unpack_from("<H", data, o)[0]

u32 = lambda o: struct.unpack_from("<I", data, o)[0]

pe = u32(0x3C)

assert data[pe:pe+4] == b"PE\0\0"

optional = pe+24

assert u16(optional) == 0x20B

sections = []

offset = optional+u16(pe+20)

for n in range(u16(pe+6)):

    at = offset+n*40

    sections.append((u32(at+12), max(u32(at+8), u32(at+16)), u32(at+20)))

def rva(address):

    for start, size, raw in sections:

        if start <= address < start+size:

            return raw+address-start

    raise AssertionError(f"Unmapped RVA {address:x}")

def string(at):

    return data[at:data.index(0, at)].decode("ascii")

exports = rva(u32(optional+112))

names = rva(u32(exports+32))

actual = {string(rva(u32(names+n*4))) for n in range(u32(exports+24))}

assert actual == {"SKSEPlugin_Load", "SKSEPlugin_Query", "SKSEPlugin_Version"}, actual

imports = []

at = rva(u32(optional+120))

while u32(at+12):

    imports.append(string(rva(u32(at+12))).lower())

    at += 20

assert "d3dcompiler_47.dll" in imports, imports

assert not any(x in name for name in imports for x in ("enb", "kiloader", "d3dcompiler_43", "46e"))

for file in (project/"src").glob("*"):

    if not file.is_file(): continue

    source = file.read_text(encoding="utf-8-sig")

    for forbidden in ("ENBGet", "DirtyHack", "TwInit", "KiLoader", "enbeffect", "enbseries.ini", "enblocal.ini"):

        assert forbidden not in source, (file, forbidden)

print("PASS: only SKSE plugin exports, compiler47, no legacy imports/SDK/preset loader; own renderer only.")

print("Imports:", ", ".join(imports))


assert not (project/"src/editor.cpp").exists()
assert not (project/"src/editor_view.cpp").exists()
assert not (project/"src/input_capture.cpp").exists()
assert b"GetAsyncKeyState" not in data and b"DirectInput8Create" not in data
assert b"SetWindowLongPtr" not in data and b"ImGui_Impl" not in data
print("PASS: no local editor, renderer backend, DirectInput hook or hotkey polling.")
