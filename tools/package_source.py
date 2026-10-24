"""Create a source-only archive from an explicit allowlist."""
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED
import hashlib
import json
root=Path(__file__).resolve().parents[1]
folders=(".github","src","shaders","tests","tools","third_party","licenses","docs")
files=(".gitignore","CMakeLists.txt","CGE.ini","LICENSE","README.md","CONTRIBUTING.md","CHANGELOG.md","THIRD-PARTY-NOTICES.txt")
selected=[root/name for name in files]
for name in folders:
    selected += [p for p in (root/name).rglob("*") if p.is_file()]
manifest=[]
for file in sorted(selected):
    relative=file.relative_to(root).as_posix()
    assert file.suffix.lower() not in (".dll",".exe",".pdb",".zip",".log",".map"),relative
    manifest.append({"path":relative,"sha256":hashlib.sha256(file.read_bytes()).hexdigest()})
output=root/"CGE-0.1alpha-source.zip"
with ZipFile(output,"w",ZIP_DEFLATED) as archive:
    for entry in manifest:
        archive.write(root/entry["path"],entry["path"])
    archive.writestr("SOURCE-MANIFEST.json",json.dumps(manifest,indent=2))
print(f"Source-only archive: {len(manifest)} files; {output.name}")
