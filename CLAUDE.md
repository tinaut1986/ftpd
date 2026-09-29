# Project notes for Claude

This is **ftpd-EX**, a fork of `mtheall/ftpd` (FTP server for 3DS/Switch/NDS). Our work
is the 3DS side: a reworked ImGui UI, touchscreen support, i18n (English/Spanish) and
assorted bug fixes. Switch/NDS/Linux code is upstream's and is not built or released
from this fork.

## Language: English in code

All **comments, function names, and variable names** are written in English.
User-facing strings live in `source/i18n.cpp` / `include/i18n.h` (English + Spanish);
never hardcode UI text in the UI code, add a `STR_*` entry instead.

## Git remotes: `origin` is the one that matters

`origin` is `tinaut1986/ftpd` (this project). `upstream` is `mtheall/ftpd`, a
**different project**: never open a PR, push, or target an issue against it unless
explicitly told to. Local `master` still tracks `upstream/master`, so a bare
`git push` would go there — always name the remote: `git push origin master`.

Because this repo is a fork, `gh` resolves bare commands to the parent by default.
`remote.origin.gh-resolved=base` (local config, not checked in) overrides that. On a
fresh clone run:

```sh
gh repo set-default tinaut1986/ftpd
```

## Building (3DS)

Needs devkitARM + portlibs `3ds-curl 3ds-mbedtls 3ds-zlib 3ds-jansson`
(`dkp-pacman -S ...`). Configure downloads GSL and Dear ImGui on first run.

```sh
scripts/build-3ds.sh [label]    # -> dist/ftpd-ex-<label>.{3dsx,cia}, dist/ftpd-classic-<label>.{3dsx,cia}
```

The script configures/builds `build-3ds` (EX, `FTPD_CLASSIC=OFF`) and
`build-3ds-classic` (`FTPD_CLASSIC=ON`), then builds the CIAs with `tools/bin/makerom`
and `bannertool` (RSFs in `meta/`). CMake itself has **no CIA target**, so
`make` alone leaves stale `.cia` files behind — always use the script (or rerun
makerom) after code changes. `build*/` and `dist/` are gitignored.

Quick 3DS-only iteration: `make -C build-3ds` produces `ftpd.elf`/`ftpd.3dsx`.

### Deploying to the console

Run ftpd on the 3DS (anonymous, port 5000 — the user's console has been at
`192.168.1.136`) and upload with `ftplib`/`curl`:

- `/3ds/ftpd-ex.3dsx` ← `ftpd-ex-*.3dsx`
- `/cias/ftpd-ex.cia`, `/cias/ftpd.cia` ← EX CIA; `/cias/ftpd-classic.cia` ← classic CIA

`430 Invalid user` means the running ftpd has a user configured; ask the user
rather than guessing credentials. CIAs are installed by the user with FBI.
The config file on the SD is `/config/ftpd/ftpd.cfg`.

## Things that have bitten us

- `hidTouchRead` returns zeros once the stylus is up. `updateTouch`
  (`source/3ds/imgui_ctru.cpp`) must release the click at the **last valid** position,
  or buttons stop working and dragging a scrollbar snaps it back to the top.
- Never write `ImGuiWindow::ScrollTarget` directly: it is a position to be centered,
  not a scroll offset. Use `ImGui::SetScrollY(window, y)`.
- `FtpSession::transferring()` drives the "active transfers" cards. It must count real
  file transfers only (listings would flash cards on/off) and keeps a 2 s hold-off.
- `BulletText` does not wrap; on the 3DS's 320px bottom screen use `bulletWrapped()`
  (`source/ftpServer.cpp`) or `TextWrapped`.
- Empty values in `ftpd.cfg` (`user=`) are valid and must not log errors.

There is no test suite and no emulator run in CI: changes to the UI/touch code need
the user to verify on hardware. Say so instead of claiming they work.

## Release process

Releases are built by `.github/workflows/build-release.yml` for tags matching
`v*-EX*` (e.g. `v3.3.0-EX`; plain `vX.Y.Z` tags belong to upstream). It builds both
variants and publishes, per tag: `ftpd-ex-<tag>.{cia,3dsx}`,
`ftpd-classic-<tag>.{cia,3dsx}`, a QR code per CIA and an auto changelog.
`.github/workflows/ci.yml` only builds on pushes to `master`/`release/*` and PRs.
The devkitARM image is pinned; bump it deliberately.

Version numbers are meaningful, not sequential: a **minor** bump marks a milestone, a
**patch** is an ordinary fix round. Ask before choosing a version or a minor bump;
never infer one. The base version is `project(ftpd VERSION ...)` in `CMakeLists.txt`
(the `-EX` suffix is `FTPD_PATCH`); keep it in step with the tag.

`master` means **stable**. Work that is not ready to be called stable stays on a
`release/*` branch. Which path applies is the user's call, so **ask**:

**Beta** — tag the unmerged branch; publishes a pre-release.

```sh
git tag -a v3.3.0-EX -m "v3.3.0-EX"
git push origin release/v3.3.0-EX
git push origin v3.3.0-EX
```

**Stable** — merge into `master` first, then tag.

```sh
git checkout master
git merge --no-ff release/v3.3.0-EX
git tag -a v3.3.0-EX -m "v3.3.0-EX"
git push origin master          # master FIRST, or the tag build publishes as a beta
git push origin v3.3.0-EX
```

The channel is decided by whether `origin/master` can reach the built commit, not by
whether a tag was pushed. To promote a shipped beta, merge into `master`, push it, and
re-run **Build 3DS Release** from the Actions tab choosing **the tag itself** as ref
(dispatching on `master` would create a wrongly named second release).
Actions uses the workflow file at the tagged commit, so workflow changes only apply to
tags cut after they landed.
