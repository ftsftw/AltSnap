#!/usr/bin/env bash
set -euo pipefail
output=${1:?Usage: package_linked_grid_win64.sh OUTPUT_DIRECTORY}
package=${PACKAGE_NAME:-AltSnap-linked-grid-win64-test}
case "$package" in
  AltSnap-linked-grid-win64-test|AltSnap-auto-tile-win64-test) ;;
  *) echo "Unsupported package name: $package" >&2; exit 1 ;;
esac
stage="$output/$package"
rm -rf "$stage"
mkdir -p "$stage/Lang"
test -s AltSnap.exe
test -s hooks.dll
cp AltSnap.exe hooks.dll License.txt "$stage/"
# The repository mixes UTF-16 blobs with normalized UTF-8 language blobs.
# Checkout conversion reports errors for the former, and depends on Git/iconv.
# Read committed text explicitly and always ship Windows-readable UTF-16LE+BOM.
python3 - "$stage" <<'PY'
import pathlib, subprocess, sys
stage = pathlib.Path(sys.argv[1])
languages = subprocess.check_output(
    ['git', 'ls-tree', '-r', '--name-only', 'HEAD', 'Lang/'], text=True
).splitlines()
for source in ['AltSnap.dni'] + [n for n in languages if n.endswith('.ini')]:
    data = subprocess.check_output(['git', 'show', 'HEAD:'+source])
    text = data.decode('utf-16') if data[:2] in (b'\xff\xfe', b'\xfe\xff') else data.decode('utf-8-sig')
    member = 'AltSnap.ini' if source == 'AltSnap.dni' else source
    (stage / member).write_bytes(b'\xff\xfe' + text.encode('utf-16le'))
PY
if [[ "$package" == AltSnap-auto-tile-win64-test ]]; then
    cp AUTO_TILE_GRID.md "$stage/"
fi
python3 - "$stage/AltSnap.ini" <<'PY'
import pathlib, sys
config = pathlib.Path(sys.argv[1]).read_bytes().decode('utf-16')
assert 'LinkedGridResize=0' in config
assert 'StickyResize=0' in config
assert not any(line.strip().startswith(('AutoTileGrid=', 'AutoTileGridSelected=')) and
               line.strip().split('=', 1)[1] != '0' for line in config.splitlines())
# Trace is opt-in even if the default template does not mention it.
assert not any(line.strip() == 'LinkedGridTrace=1' for line in config.splitlines())
assert not any(line.strip() == 'AutoTileTrace=1' for line in config.splitlines())
PY
mkdir -p "$output"
rm -f "$output/$package.zip"
(cd "$output" && zip -X -q -r "$package.zip" "$package")
python3 - "$output/$package.zip" "$package" <<'PY'
import pathlib, subprocess, sys, zipfile
archive = pathlib.Path(sys.argv[1])
with zipfile.ZipFile(archive) as z:
    names = set(z.namelist())
    root = sys.argv[2] + '/'
    required = {root + n for n in ('AltSnap.exe','hooks.dll','AltSnap.ini','License.txt')}
    if sys.argv[2] == 'AltSnap-auto-tile-win64-test':
        required.add(root + 'AUTO_TILE_GRID.md')
    assert required <= names, required - names
    languages = subprocess.check_output(
        ['git', 'ls-tree', '-r', '--name-only', 'HEAD', 'Lang/'], text=True
    ).splitlines()
    languages = [n for n in languages if n.endswith('.ini')]
    assert languages
    assert {root+n for n in languages} <= names
    for source in ['AltSnap.dni'] + languages:
        member = 'AltSnap.ini' if source == 'AltSnap.dni' else source
        data = z.read(root + member)
        committed = subprocess.check_output(['git', 'show', 'HEAD:'+source])
        text = committed.decode('utf-16') if committed[:2] in (b'\xff\xfe', b'\xfe\xff') else committed.decode('utf-8-sig')
        assert data[:2] == b'\xff\xfe', source
        assert data.decode('utf-16') == text, source
    assert all(n in required or n == root or n == root+'Lang/' or
               (n.startswith(root+'Lang/') and n.endswith('.ini')) for n in names)
    for name in required:
        assert z.getinfo(name).file_size > 0
    assert z.testzip() is None
    # Confirm x64 PE files with compiled resources, not accidental host binaries.
    import struct
    for name in ('AltSnap.exe', 'hooks.dll'):
        data = z.read(root + name)
        assert data[:2] == b'MZ'
        pe = struct.unpack_from('<I', data, 0x3c)[0]
        assert data[pe:pe+4] == b'PE\0\0'
        machine, sections = struct.unpack_from('<HH', data, pe+4)
        assert machine == 0x8664
        optional_size = struct.unpack_from('<H', data, pe+20)[0]
        assert struct.unpack_from('<H', data, pe+24)[0] == 0x20b
        table = pe + 24 + optional_size
        assert any(data[table+i*40:table+i*40+8].rstrip(b'\0') == b'.rsrc'
                   for i in range(sections))
    print(archive)
    for name in sorted(names):
        if name != root and name != root+'Lang/':
            print(name, z.getinfo(name).file_size)
PY
sha256sum "$output/$package.zip"
