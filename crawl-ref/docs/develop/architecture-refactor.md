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
- Two `game_state` queries (`is_replaying_keys` and `game_type_qualifier`)
  looked at the global `crawl_state` instead of the instance they were called
  on. They now read their own fields; tests construct independent instances.
- Command repeat cancellation now accepts an explicit player for its wizard
  exception policy. Macro buffer flushing still checks the global
  `crawl_state`; the decision is tested independently until input ownership
  moves with the command pipeline.
- Delay-driven command interruptions now pass the player and command state
  explicitly; the macro input buffer is still process-wide.
- The unused god-acting overloads that silently read `you.religion` are gone;
  callers already identify the god they are acting for.
- Zero-turn cancellation now receives the player explicitly at its call sites.
- Seed visibility receives the observing player, including on the server build.
  The legacy `cancel_cmd_repeat` overload still reads `you` for unmigrated
  callers; `state.cc` no longer reads the global `crawl_state`.
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
- Equipment slot cleanup now takes the player explicitly and invokes the
  unwield callback before clearing the weapon slot.
- Pending pickup and inscription requests now belong to `game_state` and reset
  with a game; their old `items.cc` globals and query wrappers are gone.
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
- Pass the display threshold into item message formatting. The last caller of
  the one-argument adapter has moved, so that adapter has been removed.
- Add tests for partial and complete stack consumption, equipped items, stack
  merge decisions, and inventory/level transitions.
- Preserve `items.h` entry points as adapters until callers are migrated.

### 3. Turn pipeline

- Give input decoding, command execution, world advancement, and presentation
  distinct entry points. Preserve their existing order in `main.cc` first.
- Begin with named preparation, world advancement, presentation, and turn
  bookkeeping stages within `world_reacts()`; migrate their dependencies in
  subsequent changes.
- Rest turn classification now keeps its one-turn history in `game_state`
  instead of a function-local static, and resets it when the game resets.
- The checkpoint decision now reads its own `game_state` and receives the
  completed turn and delay status explicitly; save execution remains in the
  same turn stage.
- Turn bookkeeping now receives the player, level, and game state at its
  entry point. Delay inspection and the save calls still use their existing
  global interfaces.
- Make turn and command state explicit at those boundaries. Characterize free
  actions, repeat/again, Lua callbacks, monster reactions, and save triggers.

### 4. State ownership

- Introduce a session composition root after the module APIs are explicit.
  Move the player, level, command state, RNG, and Lua lifetimes into it in
  separate changes.
- Persistent RNG streams now have a `rng::persistent_state` owner. The global
  functions remain as adapters until callers receive a stream explicitly.
  Scoped generator adapters still select streams on the process-wide instance.
- `rng::bounded`, `rng::range`, and `rng::flip` accept a specific stream. The
  legacy random helpers delegate to them with the currently selected stream.
- Stream choice and temporary subgenerator selection now live in
  `rng::selection_state`, which can use an independent persistent owner. The
  scoped generator API delegates to the process-wide instance for now.
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
- `make FORCE_PKGCONFIG=1 FULLDEBUG=YesPlease catch2-tests` passed: 8,503
  assertions in 83 test cases, including item and RNG serialization checks,
  explicit/legacy random helper parity, and independent level item iteration
  and visibility rules, independent command state queries, and turn checkpoint
  policy.
- `test/refactor/check-turn-stress.sh` runs the existing
  `test/stress/woken_rest.rc` scenario on a named Sprint map and checks that
  the character survives exactly 1,000 fixed-seed turns.
- `test/refactor/fixtures/CompatRefactor.cs` is a fixed-seed save created
  before persistent RNG ownership changed. The compatibility script loads it
  and saves after five more turns.

## Next migration gates

1. Capture fixed-seed transcripts for equipped item consumption, a combat
   exchange, a level transition, and command repeat/again. Check player state,
   level state, messages, and RNG draw counts before and after each extraction.
2. Move inventory equipment and quiver effects behind an API that takes the
   relevant player and command state. Preserve the current callback order.
   Remove the old `items.h` entry points after their callers move.
3. Give command decoding, execution, and turn bookkeeping specific input and
   state arguments. Keep UI rendering and save triggers at their existing
   points in the turn until the transcripts pass.
4. Move RNG stream selection and temporary subgenerators into a session owner;
   migrate rules to the explicit stream helpers. Then move player and level
   lifetimes, followed by configuration and Lua.
5. Add old saves from supported versions before changing `files.cc` or
   `tags.cc`. Each must load, advance, and resave in the console test harness.

Fixed-seed combat and level-transition transcripts, direct equipped-item
consumption coverage, and inventory/level transition tests remain to complete
the first two milestones. The codebase-wide refactor remains in progress.
