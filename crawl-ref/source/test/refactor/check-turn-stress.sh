#!/bin/sh
set -eu

cd "$(dirname "$0")/../.."

scratch=$(mktemp -d "${TMPDIR:-/tmp}/crawl-turn-stress.XXXXXX")
trap 'rm -rf "$scratch"' EXIT HUP INT TERM

./crawl -dir "$scratch" -builddb >"$scratch/builddb.log" 2>&1
util/fake_pty ./crawl -dir "$scratch" \
    -rc test/stress/woken_rest.rc -sprint \
    -sprint-map dungeon_sprint_mu -seed 1 \
    >"$scratch/run.log" 2>&1

dump=$(find "$scratch/morgue" -maxdepth 1 -name 'morgue-CPU_hog-*.txt' \
       -type f -print -quit)
if [ -z "$dump" ] || ! grep -Eq 'Turns:[[:space:]]+1000,' "$dump"; then
    cat "$scratch/run.log" >&2
    if [ -n "$dump" ]; then
        cat "$dump" >&2
    fi
    exit 1
fi

grep -Fq 'Game seed: 1' "$dump"
grep -Fq 'Health: 15/15' "$dump"
echo "Completed 1,000 fixed-seed Sprint turns."
