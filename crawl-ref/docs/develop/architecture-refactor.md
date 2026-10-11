# Incremental architecture refactor

This document tracks the refactor of Crawl's process-wide gameplay state. The
goal is to make each gameplay operation's inputs and effects visible while
preserving existing rules, seeded behavior, and save compatibility.

## Current boundaries

- `main.cc` constructs `you`, `env`, `crawl_state`, and both Lua interpreters.
  It also handles input, command dispatch, turn advancement, presentation, and
  save triggers.
- `items.cc` combines item storage, inventory rules, pickup UI, identification,
  and messages.
- `state.h` mixes command state, startup state, UI flags, and gameplay state.
- Tests often initialize a whole game to exercise a single rule. The player
  fixture explicitly cannot model a dungeon level.
- `random.cc` held the persistent PCG streams, selected stream, and temporary
  subgenerator as process-wide statics. Save tags serialize persistent streams
  in enum order.

In a text scan of the 307 top-level `.cc` files, 223 mention `you`, 158 mention
`env`, and 145 mention `crawl_state`. These counts are a starting measure, not
a target to optimize at the expense of clear APIs.

## Audit hot spots and order of work

| Unit | Size | Boundary to make explicit |
| --- | ---: | --- |
| `items.cc` | 4,675 lines | Level item storage, inventory changes, equipment effects, and UI prompts |
| `main.cc` + `state.cc` | 3,729 lines | Command input, turn advancement, command repetition, and save triggers |
| `random.cc` | 580 lines | Persistent stream ownership and stream choice for each rule |
| `files.cc` + `tags.cc` | 11,579 lines | Save orchestration versus versioned field encoding |
| `player.cc` + `dungeon.cc` | 15,814 lines | Player rules versus level rules, after callers have explicit state inputs |
| `initfile.cc` | 5,748 lines | Parsed options and runtime configuration versus process-wide `Options` |

The file sizes measure risk, not refactor priority. Inventory and RNG offer
smaller rule boundaries with direct tests. Turn handling follows once command
and world-reaction outcomes have fixed-seed coverage. Save orchestration and
the larger player/dungeon modules should be changed only after those inputs
are explicit; their versioned data must continue to load old files.

## Rules for the migration

1. Extract one behavior at a time. First characterize its observable results;
   then move it behind an API with explicit dependencies. Keep the old entry
   point as a temporary adapter for unmigrated callers.
2. Pass the specific player, level, configuration, RNG, or output dependency a
   function needs. Do not replace the globals with a generic context passed
   everywhere.
3. Keep turn order and RNG draw order unchanged while moving code. Save format
   changes require separate compatibility work.
4. Avoid simultaneous rule changes and architecture changes in the same patch.
5. Remove each adapter once its callers have moved. Track remaining direct
   global accesses in the migrated module.

## Milestones

### 1. Baseline and characterization

- Build the console game and run `make catch2-tests` and the scripted game
  tests in the supported toolchain.
- Capture fixed-seed command transcripts for inventory actions, one combat
  turn, a level transition, and save/load. Compare outcomes and RNG state on
  both sides of each extraction.
- Keep representative old saves for load/resave checks.

### 2. Inventory vertical slice

- Separate item quantity and stacking rules from equipment updates, command
  repetition, quiver refresh, messages, and prompts.
- Keep stack eligibility and compatibility rules in `item-stack.cc`, away from
  pickup and menu control flow.
- The stack API accepts explicit identification knowledge; the original
  overload remains an adapter for callers using game-wide item knowledge.
- Floor and monster inventory iterators accept an explicit level. Visible floor
  iteration also accepts the observing species. `items.cc` uses those
  constructors; other callers still have legacy adapters. Their implementation
  and visible floor lookup live in `item-iterators.cc`, separate from pickup
  and inventory control flow.
- Item visibility in deep water now takes species explicitly. The player
  convenience method delegates to the level/species rule, and terrain code no
  longer reads `you` to make this decision.
- Pass the display threshold into item message formatting; keep the old
  signature as an adapter while callers migrate.
- Add tests for partial and complete stack consumption, equipped items, stack
  merge decisions, and inventory/level transitions.
- Preserve `items.h` entry points as adapters until callers are migrated.

### 3. Turn pipeline

- Give input decoding, command execution, world advancement, and presentation
  distinct entry points. Preserve their existing order in `main.cc` first.
- Begin with named preparation, world advancement, presentation, and turn
  bookkeeping stages within `world_reacts()`; migrate their dependencies in
  subsequent changes.
- Make turn and command state explicit at those boundaries. Characterize free
  actions, repeat/again, Lua callbacks, monster reactions, and save triggers.

### 4. State ownership

- Introduce a session composition root after the module APIs are explicit.
  Move the player, level, command state, RNG, and Lua lifetimes into it in
  separate changes.
- Persistent RNG streams now have a `rng::persistent_state` owner. The global
  functions remain as adapters until callers receive a stream explicitly.
  Stream selection and temporary subgenerators still use process-wide state.
- `rng::bounded`, `rng::range`, and `rng::flip` accept a specific stream. The
  legacy random helpers delegate to them with the currently selected stream.
- Keep serialization at the boundary, with compatibility tests for old saves.

## Baseline setup on this macOS checkout

The bundled Lua submodule must be initialized. The generator scripts also need
PyYAML. The default macOS Makefile path expects bundled zlib and SQLite; using
`FORCE_PKGCONFIG=1` allows installed system libraries for a console build.
The vendored Catch2 debugger trap needed an Apple Silicon fallback before its
test runner could compile.

## Verified on 2026-10-10

- Console game built with `FORCE_PKGCONFIG=1 FULLDEBUG=YesPlease`.
- After `crawl -dir <writable-directory> -builddb`, the game's `-test -seed 1`
  suite completed successfully.
- `make FORCE_PKGCONFIG=1 FULLDEBUG=YesPlease catch2-tests` passed: 8,445
  assertions in 72 test cases, including item and RNG serialization checks,
  explicit/legacy random helper parity, and independent level item iteration
  and visibility rules.
- The existing `test/stress/woken_rest.rc` sprint scenario completed 1,000
  automated turns under `util/fake_pty` after the turn-stage split.
- `test/refactor/fixtures/CompatRefactor.cs` is a fixed-seed save created
  before persistent RNG ownership changed. The compatibility script loads it
  and saves after five more turns.

Fixed-seed command transcripts for combat and level transitions, equipped-item
tests, and inventory/level transition tests remain before the first two
milestones are complete.
