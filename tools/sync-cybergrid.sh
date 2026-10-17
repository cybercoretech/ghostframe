#!/bin/sh
# Copies the CYBERGRID palettes from the cybercore framework repo (the source
# of truth) into assets/cybergrid/ and regenerates cybergrid.qrc.
#   tools/sync-cybergrid.sh [path/to/cybercore]
set -eu
here=$(cd "$(dirname "$0")/.." && pwd)
src=${1:-"$here/../../framework/cybercore"}/schema/themes
[ -d "$src" ] || { echo "no themes at $src" >&2; exit 1; }
dest="$here/assets/cybergrid"
rm -rf "$dest"
mkdir -p "$dest"
for family in "$src"/*/; do
    name=$(basename "$family")
    mkdir -p "$dest/$name"
    cp "$family"*.json "$dest/$name/"
done
cat > "$dest/README.md" <<'NOTE'
# CYBERGRID palettes

Copied from [cybercore-tech/cybercore](https://github.com/cybercoretech/cybercore)
`schema/themes/` (MIT) by `tools/sync-cybergrid.sh`. Don't edit here: change
them in cybercore and re-run the script. Ghostframe uses them for its
CYBERGRID gradient maps, and also reads custom themes from
`~/.config/cybercore/themes/` at runtime.
NOTE
{
    echo '<RCC>'
    echo '    <qresource prefix="/cybergrid">'
    (cd "$dest" && find . -name '*.json' | sort | sed 's|^\./||') | while read -r f; do
        echo "        <file alias=\"$f\">assets/cybergrid/$f</file>"
    done
    echo '    </qresource>'
    echo '</RCC>'
} > "$here/cybergrid.qrc"
echo "synced $(find "$dest" -name '*.json' | wc -l) palettes from $src"
