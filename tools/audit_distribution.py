                                                                                       

import hashlib

import json
import re

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

ordinals = rva(u32(exports+36))
functions = rva(u32(exports+28))
export_addresses = {
    string(rva(u32(names+n*4))): u32(functions+4*u16(ordinals+n*2))
    for n in range(u32(exports+24))
}
metadata_rva = export_addresses["SKSEPlugin_Version"]
assert not u32(optional+112) <= metadata_rva < u32(optional+112)+u32(optional+116)
metadata = rva(metadata_rva)
assert metadata+0x350 <= len(data), "Truncated static SKSE metadata"
assert u32(metadata) == 1, "Unsupported SKSE metadata schema"
version_match = re.search(r"project\(CommunityGraphicsEngine VERSION (\d+)\.(\d+)\.(\d+)", (project / "CMakeLists.txt").read_text())
assert version_match, "Missing project version"
major, minor, patch = map(int, version_match.groups())
assert u32(metadata+4) == (major << 24) | (minor << 16) | (patch << 8), "SKSE plugin version differs from project version"
compatibility = struct.unpack_from("<16I", data, metadata+0x30C)
assert 0 in compatibility, "SKSE version list has no terminator"
listed = compatibility[:compatibility.index(0)]
assert len(set(listed)) == len(listed), "Duplicate SKSE version declarations"
assert not any(compatibility[len(listed):]), "Versions after SKSE list terminator are unreachable"
family = int(sys.argv[3]) if len(sys.argv) > 3 else 0
assert family in (0, 5, 6, 7)
pack = lambda minor, build: (1 << 24) | (minor << 16) | (build << 4)
expected = [pack(5, n) for n in (3,16,23,39,50,53,62,73,80,97)] if family == 5 else []
if family in (0, 6):
    expected += [pack(6, n) for n in (317,318,323,342,353,629,640,659,1130,1170,1179)]
if family in (0, 7):
    expected += [pack(7, n) for n in (99,104)]
if family in (0, 6):
    expected += [pack(6, n) | 1 for n in (659,1179)]
assert list(listed) == expected, (family, listed, expected)
assert 0x01064921 not in listed, "Unverified GOG 1.6.1170 must not be declared compatible"
assert not u32(metadata+0x308) & ~7, "Unsupported SKSE independence flags"
assert u32(metadata+0x304) & 2, "Missing Address Library V5 declaration"
assert u32(metadata+0x34C) <= 0x02020020, "Older GOG SKSE blocked by minimum SKSE version"
print(f"PASS: static SKSE metadata declares exactly the expected runtime family ({family or 'internal'}).")

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

assert (project/"LICENSE").read_bytes() in data
print("PASS: project license is embedded in CGE.dll.")

assert (project/"THIRD-PARTY-NOTICES.txt").read_bytes() in data
print("PASS: third-party license notices are embedded in CGE.dll.")
