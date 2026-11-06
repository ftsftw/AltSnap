#!/usr/bin/env bash
set -euo pipefail
output=${1:?Usage: package_linked_grid_win64.sh OUTPUT_DIRECTORY}
package=AltSnap-linked-grid-win64-test
stage="$output/$package"
rm -rf "$stage"
mkdir -p "$stage/Lang"
test -s AltSnap.exe
test -s hooks.dll
cp AltSnap.exe hooks.dll License.txt "$stage/"
cp AltSnap.dni "$stage/AltSnap.ini"
cp Lang/*.ini "$stage/Lang/"
python3 - "$stage/AltSnap.ini" <<'PY'
import pathlib, sys
config = pathlib.Path(sys.argv[1]).read_bytes().decode('utf-16')
assert 'LinkedGridResize=0' in config
assert 'StickyResize=0' in config
PY
mkdir -p "$output"
rm -f "$output/$package.zip"
(cd "$output" && zip -X -q -r "$package.zip" "$package")
python3 - "$output/$package.zip" <<'PY'
import pathlib, sys, zipfile
archive = pathlib.Path(sys.argv[1])
with zipfile.ZipFile(archive) as z:
    names = set(z.namelist())
    root = 'AltSnap-linked-grid-win64-test/'
    required = {root + n for n in ('AltSnap.exe','hooks.dll','AltSnap.ini','License.txt')}
    assert required <= names, required - names
    assert any(n.startswith(root + 'Lang/') and n.endswith('.ini') for n in names)
    assert all(n in required or n == root or n == root+'Lang/' or
               (n.startswith(root+'Lang/') and n.endswith('.ini')) for n in names)
    for name in required:
        assert z.getinfo(name).file_size > 0
    print(archive)
    for name in sorted(names):
        if name != root and name != root+'Lang/':
            print(name, z.getinfo(name).file_size)
PY
sha256sum "$output/$package.zip"
