#!/bin/sh
set -eu

cd "$(dirname "$0")/../.."

scratch=$(mktemp -d "${TMPDIR:-/tmp}/crawl-save-compat.XXXXXX")
trap 'rm -rf "$scratch"' EXIT HUP INT TERM
mkdir -p "$scratch/saves"
cp test/refactor/fixtures/CompatRefactor.cs "$scratch/saves/"

./crawl -dir "$scratch" -builddb >/dev/null 2>&1
util/fake_pty ./crawl -dir "$scratch" \
    -rc test/refactor/save-to-turn-10.rc -name CompatRefactor \
    >"$scratch/run.log" 2>&1

dump="$scratch/morgue/CompatRefactor.txt"
if ! grep -Eq 'Turns:[[:space:]]+10,' "$dump"; then
    cat "$scratch/run.log" >&2
    cat "$dump" >&2
    exit 1
fi

grep -Fq 'Health: 15/15      AC:  3    Str: 15' "$dump"
grep -Fq 'c - a wand of flame (15) (quivered)' "$dump"

test -s "$scratch/saves/CompatRefactor.cs"
echo "Loaded the pre-RNG-extraction save and saved it again at turn 10."
