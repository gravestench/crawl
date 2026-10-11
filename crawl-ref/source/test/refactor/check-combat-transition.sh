#!/bin/sh
set -eu

cd "$(dirname "$0")/../.."

scratch=$(mktemp -d "${TMPDIR:-/tmp}/crawl-combat-transition.XXXXXX")
trap 'rm -rf "$scratch"' EXIT HUP INT TERM

./crawl -dir "$scratch" -builddb >"$scratch/builddb.log" 2>&1
util/fake_pty ./crawl -dir "$scratch" \
    -rc test/refactor/combat-transition.rc -seed 1 \
    >"$scratch/run.log" 2>&1

dump=$(find "$scratch/morgue" -maxdepth 1 \
       -name 'morgue-Refactor_combat-*.txt' -type f -print -quit)
if [ -z "$dump" ] || ! grep -Eq 'Turns:[[:space:]]+80,' "$dump"; then
    cat "$scratch/run.log" >&2
    if [ -n "$dump" ]; then
        cat "$dump" >&2
    fi
    exit 1
fi

grep -Fq 'Game seed: 1' "$dump"
grep -Fq 'Health: 249/249' "$dump"
grep -Fq 'You were on level 1 of the Abyss.' "$dump"
grep -Fq '43 | Abyss:1  | Cast into level 1 of the Abyss' "$dump"
grep -Fq 'A small abomination (Abyss:1)' "$dump"
grep -Fq 'c - a +9 demon whip of chaos (weapon)' "$dump"
echo "Completed fixed-seed Abyss transition and melee combat at turn 80."
