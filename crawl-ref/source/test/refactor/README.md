# Refactor regressions

`fixtures/CompatRefactor.cs` was created by the console game before the
persistent RNG state extraction. It is a seeded (`1`) Mummy Artificer save at
turn 5 after five wait commands. The binary used the refactor branch's item
stack changes but the original RNG implementation. Its save header reports
`0.30-a0-26-g351d7250ef` because the version string was generated when that
binary's build started.

From `source/`, run `test/refactor/check-save-compat.sh` after building
`crawl` and `util/fake_pty`. The script copies the fixture to a temporary game
directory, loads it, waits five more turns, and checks that turn 10 can be
saved. The fixture must remain unchanged so later refactors keep testing the
same old save.

`check-turn-stress.sh` runs a seeded 1,000-turn Sprint session. The
`check-combat-transition.sh` script runs `combat-transition.rc` with seed 1,
enters Abyss:1, and checks the turn-80 character file for the level transition,
equipped weapon, HP, and a melee kill. These scripts require the console game
and `util/fake_pty` to be built first.
