# Project notes for Claude

This is **ftpd-EX**, a fork of `mtheall/ftpd` (FTP server for 3DS/Switch/NDS). Our work
is the 3DS and Switch side: a reworked ImGui UI (`FtpServer::draw`, with a dual-screen
layout for the 3DS and its own landscape layout for the Switch), touchscreen support,
i18n (English/Spanish) and assorted bug fixes. NDS/Linux code is upstream's and is not
built or released from this fork.

## Language: English in code

All **comments, function names, and variable names** are written in English.
User-facing strings live in `source/i18n.cpp` / `include/i18n.h` (English + Spanish);
never hardcode UI text in the UI code, add a `STR_*` entry instead. Spanish strings
need their real accents and `ñ` (both consoles' fonts have them); the language names
come from `getLanguageName`.

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

## Building

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

Switch: needs devkitA64 + libnx and the portlibs
`switch-curl switch-libzstd switch-jansson switch-zlib switch-mbedtls switch-glm`, plus
ImageMagick (`convert`) and `zstd` for the texture assets.

```sh
scripts/build-switch.sh [label] # -> dist/ftpd-ex-<label>.nro, dist/ftpd-classic-<label>.nro
```

It builds `build-switch` (EX) and `build-switch-classic`. If `/opt/devkitpro` is owned by
root, `dkp-pacman` needs root; `fakeroot dkp-pacman -S ...` works once the tree belongs to
your user.

### Deploying to the console

Run ftpd on the 3DS (anonymous, port 5000 — the user's console has been at
`192.168.1.136`) and upload with `ftplib`/`curl`:

- `/3ds/ftpd-ex.3dsx` ← `ftpd-ex-*.3dsx`
- `/cias/ftpd-ex.cia`, `/cias/ftpd.cia` ← EX CIA; `/cias/ftpd-classic.cia` ← classic CIA

The Switch runs the same ftpd on port 5000 (last seen at `192.168.1.138`); upload the
`.nro` to `/switch/ftpd-ex.nro`. You cannot overwrite or delete the file of the program that
is currently running (`450`/`550 I/O error`, or a bogus `No such file`): upload under another
name, or ask the user to close it first. NROs have page-aligned sizes, so a same-size upload
is not proof of the new build; compare checksums.

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
- ImGui sets `ActiveId == window->MoveId` when a press lands on empty space, even in
  windows that cannot move. That is not a widget grabbing the touch: `TouchScroller` must
  not treat it as one, or horizontal drag-scrolling never starts.
- On the Switch, Y must not reach ImGui as `ImGuiKey_GamepadFaceLeft`: holding it opens
  ImGui's window switcher (a one-entry popup) and tapping it only focuses a menu bar.
  `imgui_nx.cpp` sends it as `ui::KEY_Y` instead.
- The 3DS system font reports whole glyph cells as glyph boxes, so text cannot be centered
  from font metrics. Badge symbols (Y, X, A, B, L, R, +, -) are drawn as vector strokes in
  `source/ui.cpp`.
- Sizes designed for the 3DS are scaled with `ui::px()` (font size / 13); do not hardcode
  pixel sizes in code that also runs on the Switch.
- `BulletText` does not wrap; on the 3DS's 320px bottom screen use `bulletWrapped()`
  (`source/ftpServer.cpp`) or `TextWrapped`.
- Empty values in `ftpd.cfg` (`user=`) are valid and must not log errors.

There is no test suite and no emulator run in CI: changes to the UI/touch code need
the user to verify on hardware. Say so instead of claiming they work.

## Release process

Releases are built by `.github/workflows/build-release.yml` for tags matching
`v*-EX*` (e.g. `v1.0.0-EX`; plain `vX.Y.Z` tags belong to upstream). It builds the
3DS (devkitARM) and Switch (devkitA64) variants in separate jobs, then publishes, per tag:
`ftpd-ex-<tag>.{cia,3dsx,nro}`, `ftpd-classic-<tag>.{cia,3dsx,nro}` (the `.nro` from the
Switch job), a QR code per CIA and an auto changelog.
`.github/workflows/ci.yml` only builds (both consoles) on pushes to `master`/`release/*`
and PRs. The devkitARM/devkitA64 images are pinned; bump them deliberately.

Version numbers are meaningful, not sequential: a **minor** bump marks a milestone, a
**patch** is an ordinary fix round. Ask before choosing a version or a minor bump;
never infer one. The base version is `project(ftpd VERSION ...)` in `CMakeLists.txt`
(the `-EX` suffix is `FTPD_PATCH`); keep it in step with the tag. This fork has its own
numbering starting at `v1.0.0-EX`, independent of upstream. The upstream version it is
based on is `FTPD_UPSTREAM_VERSION` in `CMakeLists.txt` (shown in the About tab): update
it by hand whenever upstream is merged in.

`master` means **stable**. Work that is not ready to be called stable stays on a
`release/*` branch. Which path applies is the user's call, so **ask**:

**Beta** — tag the unmerged branch; publishes a pre-release.

```sh
git tag -a v1.1.0-EX -m "v1.1.0-EX"
git push origin release/v1.1.0-EX
git push origin v1.1.0-EX
```

**Stable** — merge into `master` first, then tag.

```sh
git checkout master
git merge --no-ff release/v1.1.0-EX
git tag -a v1.1.0-EX -m "v1.1.0-EX"
git push origin master          # master FIRST, or the tag build publishes as a beta
git push origin v1.1.0-EX
```

The channel is decided by whether `origin/master` can reach the built commit, not by
whether a tag was pushed. To promote a shipped beta, merge into `master`, push it, and
re-run **Build 3DS Release** from the Actions tab choosing **the tag itself** as ref
(dispatching on `master` would create a wrongly named second release).
Actions uses the workflow file at the tagged commit, so workflow changes only apply to
tags cut after they landed.
