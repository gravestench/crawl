#!/bin/sh
set -eu

cd "$(dirname "$0")/../.."

scratch=$(mktemp -d "${TMPDIR:-/tmp}/crawl-repeat-again.XXXXXX")
trap 'rm -rf "$scratch"' EXIT HUP INT TERM

./crawl -dir "$scratch" -builddb >"$scratch/builddb.log" 2>&1
util/fake_pty ./crawl -dir "$scratch" \
    -rc test/refactor/repeat-again.rc -seed 1 \
    >"$scratch/run.log" 2>&1

dump="$scratch/morgue/RepeatRefactor.txt"
if [ ! -s "$dump" ] || ! grep -Eq 'Turns:[[:space:]]+4,' "$dump"; then
    cat "$scratch/run.log" >&2
    if [ -f "$dump" ]; then
        cat "$dump" >&2
    fi
    exit 1
fi

grep -Fq 'Game seed: 1' "$dump"
grep -Fq 'Health: 15/15' "$dump"
grep -Fq 'Number of times to repeat, then command key: 3' "$dump"
test -s "$scratch/saves/RepeatRefactor.cs"
echo "Repeated three waits, redid one, and saved at turn four."
