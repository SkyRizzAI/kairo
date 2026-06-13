<!-- Use a Conventional Commit style title, e.g. "feat(skyrizz-e32): add IMU driver" -->

## What & why

<!-- What does this change do, and why? Link any related issue: Closes #123 -->

## Type of change

- [ ] Bug fix
- [ ] New feature (app / core)
- [ ] New board or driver
- [ ] Refactor / cleanup
- [ ] Docs
- [ ] Build / CI / tooling

## How I verified

<!-- e.g. `bun test` passes; ran in simulator; flashed to skyrizz-e32 and saw X -->

- [ ] `bun test` passes (host build + ctest)
- [ ] Verified in the simulator and/or on hardware (state which board)

## Conventions checklist

- [ ] Logging goes through `rt.log()` (no raw `Serial`/`printf`/`ESP_LOGx`)
- [ ] No branching on board type — capabilities checked via `rt.capabilities()`
- [ ] Screens use `input::Action`; footer labels via `rt.input().hintFor(...)`
- [ ] UI draws from `canvas.width()`/`height()` — no hardcoded dimensions
- [ ] New pins live in the board's `board_config.h`, not scattered in drivers
- [ ] Code is `clang-format`-clean and follows existing style
