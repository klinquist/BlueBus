# BlueBus CD53 Development Learnings

This document records the work completed and the technical conclusions reached
during the CD53 button, OBC alert, simulator, and macOS firmware-build effort.
It is intended to be a durable handoff for future development and vehicle
testing.

Status snapshot: 2026-08-27

## Executive Summary

The work produced four related outcomes:

1. A fix for a Business CD paired-device selection regression, isolated on its
   own branch and submitted upstream as PR #244.
2. Configurable CD53 cold-oil and low-voltage warnings, isolated on a separate
   firmware feature branch and pushed only to the user's fork.
3. A native terminal simulator that compiles and runs production CD53, menu,
   event, and I-Bus handling code with host-side hardware adapters.
4. A tested macOS shell script that builds an application-only firmware HEX
   using the MPLAB X and XC16 versions declared by the project.

No PR was created for the OBC alert feature, simulator, or build script. The
existing device-selection PR was not updated with those unrelated changes.

## Repository and Branch Layout

The configured remotes are:

| Remote | Repository | Purpose |
|---|---|---|
| `origin` | `git@github.com:tedsalmon/BlueBus` | Upstream project |
| `fork` | `git@github.com:klinquist/BlueBus.git` | User fork |

The relevant branches are:

| Branch | Current tip | Purpose |
|---|---|---|
| `fix-cd53-device-selection` | `8812192` | Focused fix for CD53 button 5/device selection |
| `feature-cd53-obc-alerts` | `e3a6ae3` | Cold-oil and low-voltage firmware feature |
| `feature-cd53-emulator` | `930bfca` | OBC feature plus native simulator and macOS build script |

`feature-cd53-emulator` merged `feature-cd53-obc-alerts` at `d76286b`. The
device-selection fix remains a separate upstream contribution, although the
simulator branch contains the same behavioral corrections so the simulator can
exercise the intended device-selection workflow.

Two local clones were kept synchronized:

- `/Users/kris/BlueBus`
- `/Users/kris/code/BlueBus`

The second path is the clone normally used from the user's shell.

## Business CD Button Behavior

The six CD buttons are handled as programmable inputs while BlueBus is acting
as the CD changer. Their meaning depends on the current CD53 UI mode.

| Button | Active-mode behavior | Context-dependent behavior |
|---|---|---|
| 1 | Toggle Bluetooth playback | Redisplay current text outside active mode |
| 2 | Cycle Metadata On -> Metadata Off -> OBC -> Metadata On | Edit/save in Settings; connect selected phone in Devices |
| 3 | Accept/end a call; rapid second press toggles voice recognition while idle | Requires HFP enabled |
| 4 | Enter or leave Settings | Ignored when the display is off |
| 5 | Enter or leave paired-device selection | Ignored when the display is off |
| 6 | Toggle Bluetooth discoverability/pairing | Enabling pairing closes the active connection first |

The next/previous controls also change meaning by mode:

- Active mode: next/previous Bluetooth track.
- Settings mode: next/previous setting or value.
- Device mode: next/previous paired phone.

The CD53 has an 11-character display. Menu labels and status strings must be
designed around that hard limit; longer strings may scroll in some contexts,
but a setting label whose distinguishing suffix is beyond character 11 is
ambiguous to a user.

## Button 5 Device-Selection Regression

### Expected behavior with two paired phones

1. Press CD button 5 to enter Devices.
2. After the temporary `Devices` banner expires, the first phone is shown.
3. Next moves from phone 0 to phone 1 and then wraps to phone 0.
4. Previous navigates in the opposite direction and wraps correctly.
5. CD button 2 connects the selected phone.
6. A phone is marked as active only when Bluetooth reports it as connected.
7. Selecting the already connected phone reports `Connected`; selecting a
   disconnected phone initiates a connection even if its stored index happens
   to match the stale active-device index.

### What was broken

The single-line UI refactor in upstream commit
`e1c3f44a8bb1e8c1fdfb4db91c8947b792fa26da` moved paired-device browsing into
the shared menu implementation but lost several CD53-specific invariants:

- CD button 2 no longer called the connection action in device mode.
- The initial cursor was not reset before opening the list.
- Forward wrap compared against `pairedDevicesCount`, allowing an index equal
  to the count and therefore one element beyond the valid array range.
- The active marker and `Connected` decision relied on an index without also
  verifying that the device was actually connected.

### Fix

Commit `8812192` restores the connection action, initializes the cursor with a
sentinel, corrects forward wrapping, and checks connection status before
treating an index as active.

The focused fix is upstream PR
[#244, “Restore CD53 device selection”](https://github.com/tedsalmon/BlueBus/pull/244).
Its description explicitly identifies `e1c3f44` as the breaking commit. At the
date of this document, the PR is open.

## CD53 OBC Alerts

The ordinary CD53 OBC display remains coolant temperature plus vehicle speed:

```text
C:194 S:0
```

The alert feature adds two optional exceptions without changing that normal
display format.

### Cold-oil display

The cold-oil behavior is:

- Setting: `ColdOilDisp` / `CONFIG_SETTING_COLD_OIL_DISPLAY`.
- EEPROM address: `0x1A`.
- Default in the 1.4.41 upgrade: Off.
- CD53 Settings label: `CldOil: On` or `CldOil: Off`.
- Effective only on CD53.
- Displayed only while the OBC view is active.
- Requires a valid oil-temperature reading greater than zero.
- Displays `OIL COLD` below 160 degrees Fahrenheit.
- Does not globally override unrelated temporary or mode-specific text.

Oil temperature is stored as whole degrees Celsius. The threshold constant is
72 C, described as the first whole-degree value that is not below 160 F. Thus:

- 71 C (159.8 F) is cold.
- 72 C (161.6 F) is not cold.
- Zero is treated as missing/invalid data, not cold oil.

An early implementation hid the setting unless
`menuContext.activeView == MENU_SINGLELINE_VIEW_OBC`. That attempted to model
the dependency on Display OBC, but it made the setting inaccessible or
confusing from the normal metadata view. The `activeView` gate was removed.
The setting is now visible for every CD53 user, while its display effect still
naturally depends on the OBC view because `MenuSingleLineOBC()` renders it.

### Low-voltage warning

The low-voltage behavior is:

- Setting: `LowVoltWrn` / `CONFIG_SETTING_LOW_VOLT_WARNING`.
- EEPROM address: `0x1B`.
- Default in the 1.4.41 upgrade: On.
- CD53 Settings label: `LowV: On` or `LowV: Off`.
- Effective only on CD53.
- Requires a valid nonzero battery-voltage reading.
- Activates below 13.3 V; exactly 13.3 V is not low.
- Activates only while the engine is considered running.
- Displays the measured value, for example `LOW V13.2`.
- Overrides all other CD53 text, including `OIL COLD` and temporary messages.
- Clears and restores normal display processing after voltage recovery.

The firmware derives engine-running state from the IKE speed/RPM frame:
`pkt[IBUS_PKT_DB2] > 0`. Ignition-off also explicitly clears the state. This is
more accurate than treating ignition-on as engine-running, but it means the
warning's definition of “running” is specifically “a nonzero RPM sample has
been received.”

Low voltage is implemented in the top-level CD53 display timer rather than in
the OBC renderer. That location is what makes it a true global priority
override independent of the current metadata/OBC/settings text.

### Settings UI lesson

The original labels `ColdOilDisp: On/Off` and `LowVoltWrn: On/Off` exceeded the
11-character screen. The user could change the value internally, but the
visible portion stopped before `On` or `Off`, making the operation appear
broken. The shortened `CldOil` and `LowV` labels make state visible on one
screen.

### Firmware version migration

The feature increments firmware to 1.4.41. The 1.4.41 upgrade initializes only
the two new EEPROM values:

- Cold oil: Off.
- Low voltage: On.

Existing settings remain in EEPROM. The application HEX does not contain or
replace the bootloader.

## Native CD53 Simulator

The original planning considered a browser emulator and multiple possible
renderers. The chosen first implementation is a native curses TUI because it
is lightweight, easy to run beside the source tree, and well suited to rapid
firmware-state testing.

Run it with:

```sh
make -C simulator run
```

Run its regression suite with:

```sh
make -C simulator test
```

### Architecture

The simulator compiles these production components directly:

- `firmware/application/ui/cd53.c`
- `firmware/application/ui/menu/menu_singleline.c`
- `firmware/application/lib/event.c`
- `firmware/application/lib/ibus.c`

Only hardware boundaries are replaced:

- EEPROM/configuration is held in memory.
- Timer progression is deterministic.
- Bluetooth commands update simulated state.
- I-Bus transmissions are captured as display output and trace data.
- Minimal host definitions replace PIC24-specific headers and runtime hooks.

`IBusProcessFrame()` was extracted from the physical UART loop so both hardware
and simulator paths validate frame length/checksum and dispatch packets through
the same production handlers. Simulator sensor controls construct complete,
checksum-valid I-Bus frames instead of directly assigning decoded fields.

`EventReset()` was added so a simulated reset can safely rebuild callback state
without retaining duplicate registrations.

`simulator/src/bluebus_sim.c` is renderer-neutral. The curses UI is only one
consumer, so a graphical or browser renderer could be added later without
rewriting the firmware engine.

### Controls

| Key | Action |
|---|---|
| `1`-`6` | Corresponding CD button |
| `n` / `p` | Next / previous |
| `e` / `x` / `k` | Start engine / stop engine / key off |
| `r` / `s` | Set RPM / speed |
| `o` / `c` | Set oil / coolant temperature |
| `v` | Set voltage |
| `O` / `V` | Directly toggle cold-oil / low-voltage settings |
| `m` | Set metadata |
| `d` | Load two paired phones |
| `a` / `l` | Load cold-oil / low-voltage presets |
| `R` | Reset simulation |
| `t` / `q` | Toggle trace / quit |

The uppercase setting shortcuts are case-sensitive. Lowercase `o` and `v` edit
oil temperature and voltage instead.

### Simulator-discovered bugs

The simulator initially displayed `ENGINE: STOPPED RPM: 800`. Internally, 800
RPM was the configured idle target, but exposing that target as a measured RPM
while stopped created an impossible state. `BlueBusSimGetRPM()` now reports
zero unless `SimEngineRunning` is true. Tests cover stopped -> running at 800 ->
stopped at zero.

The simulator also made the settings-label truncation and `activeView` gate
visible. This was a genuine firmware usability issue rather than a TUI-only
problem because the TUI exercises the production menu and CD53 code.

Temporary CD53 banners matter in integration tests. For example, the
`Settings` banner lasts two seconds and can mask a main-display update even
though navigation state has already changed. Tests advance virtual time before
asserting the underlying setting label.

### Test coverage

The host regression test covers:

- Initial stopped state and zero displayed RPM.
- Engine start/stop RPM transitions.
- OBC selection through CD button 2.
- Cold-oil setting visibility without selecting OBC first.
- Full edit/save flow through CD button 2 and next/previous.
- One-screen `CldOil` and `LowV` labels.
- Cold-oil boundary at 71/72 C.
- Low-voltage boundary at 13.2/13.3 V.
- Low-voltage engine-running gate.
- Low-voltage setting toggle, priority, and recovery.
- Ignition shutdown and restart.
- Two-phone button 5 browsing.
- Generated RX and production TX frame capture.

GitHub Actions builds and tests the simulator on Linux. The checkout action was
updated to v7 after the first workflow run. All recorded runs for simulator
commits `72a78da`, `20ee6d0`, `10cb509`, and `483e1b8` passed.

### Simulator limitations

This is a behavior-level firmware simulator, not a PIC24 instruction or
electrical simulator. It does not model:

- I-Bus electrical arbitration or collisions.
- Physical UART timing and noise.
- Real BM83/BC127 protocol timing and failure modes.
- Audio DAC, DSP, amplifier, or RF behavior.
- Every vehicle/module variation.
- Arbitrary raw-frame replay.

Real-vehicle validation is still required before treating the feature as
production proven.

## macOS Firmware Build

The installed and tested toolchain is:

- MPLAB X 6.35.
- XC16 2.10.
- `PIC24F-GA-GB_DFP` 1.9.336.
- Target `PIC24FJ1024GA606`.

The project intentionally ignores MPLAB-generated Makefiles and build output.
Running `make` immediately after a fresh clone therefore fails until MPLAB's
`prjMakefilesGenerator.sh` recreates `nbproject/Makefile-*.mk`.

The reusable build entry point is:

```sh
./scripts/build-firmware.sh
```

An explicit destination may be supplied:

```sh
./scripts/build-firmware.sh ~/Desktop/bluebus-test.hex
```

The script:

1. Locates the repository independent of the caller's current directory.
2. Reads the configuration, XC16 version, and device-pack version from
   `configurations.xml`.
3. Locates MPLAB X or accepts `MPLAB_MAKEGEN` as an override.
4. Verifies the configured XC16 compiler.
5. Installs the pinned device pack if it is missing.
6. Generates MPLAB Makefiles.
7. Performs a clean production build.
8. Verifies that `application.production.hex` exists and is nonempty.
9. Copies it to a version/commit-named application HEX.
10. Prints the firmware version, source commit, path, and SHA-256 checksum.

The script marks the filename/commit as `-dirty` if tracked or untracked source
changes are present. This prevents an uncommitted build from looking as though
it came exactly from the named commit.

The device-pack CLI in MPLAB X 6.35 emitted a Java warning about a missing
Apache Commons Compress class while restoring archive permissions, but it
still installed the pack successfully. The build script verifies that the
expected pack directory exists after installation rather than trusting only
the tool's log text.

The script passed POSIX shell syntax checks, ShellCheck, and a complete XC16
production build. That build used 184,368 bytes (17%) of program memory and
5,286 bytes (16%) of data memory.

For the firmware source at `483e1b8` (unchanged by the later build-script-only
commit), the tested application HEX was 670 KB with SHA-256:

```text
b75aa70753cc28b5b3c6d0cbff9ec30421ba53c64856b3460df12b0a4777b11b
```

Build outputs live under
`firmware/application/dist/application/production/` and are ignored by Git.

## Flashing Notes

No BlueBus module was connected to the Mac during this work, and no hardware
was flashed. Compilation does not require the module.

For transfer to another computer, use the named `.hex` produced by the build
script. It is an application-only image suitable for the BlueBus USB
bootloader. Do not flash the `.elf`, `.map`, or anything under
`firmware/bootloader/` during a normal application update.

The official browser firmware tool can load a local HEX. A command-line upload
is also possible with `utility/console_firmware_tool.py`, but the repository's
console tool expects the module to already be in bootloader mode and its actual
firmware option is `--firmware`, not the stale `--flash` example found in some
documentation. The running application's serial CLI accepts `bootloader` to
set the EEPROM boot flag and reset into the bootloader.

Keep a known-good official application HEX available for rollback and do not
disconnect power or USB during erase/write.

## Git Workflow Lessons

The error:

```text
error: pathspec 'feature-cd53-emulator' did not match any file(s) known to git
```

occurred because the requested branch was not yet present in that clone's
local refs. A branch pushed to a fork is not automatically known to another
clone. The fix was to configure/fetch the `fork` remote, then create or switch
to a local branch tracking `fork/feature-cd53-emulator`.

A typical recovery sequence is:

```sh
git remote add fork git@github.com:klinquist/BlueBus.git  # only if missing
git fetch fork
git switch --track fork/feature-cd53-emulator
```

Once the local tracking branch exists, this is sufficient:

```sh
git switch feature-cd53-emulator
git pull --ff-only fork feature-cd53-emulator
```

Feature isolation was important throughout this work:

- The device-selection regression fix went to its own branch and upstream PR.
- OBC alerts stayed on a separate branch in the user fork with no PR.
- The simulator branched from the alert work and later merged its follow-up.
- Alert and simulator changes were never added to PR #244.

## Potential Future I-Bus Displays

The existing I-Bus state and handlers suggest several future CD53 display
ideas, subject to vehicle/module availability and the 11-character limit:

- Oil temperature as a normal numeric page once a valid reading exists.
- Battery voltage as an optional normal page in addition to the warning.
- Ambient temperature.
- Engine RPM.
- Estimated range or other OBC text properties already broadcast by the IKE.
- Time/date or synchronized clock status.
- PDC nearest-obstacle distance or directional summaries.
- Door, trunk, lock, or lighting status as short temporary alerts.
- Ignition/engine state diagnostics.
- Active phone, call state, or Bluetooth connection diagnostics.
- Raw I-Bus module-presence/service diagnostics for development builds.

Good candidates should have explicit invalid/stale-data handling, restrained
update rates, readable 11-character abbreviations, and a defined priority
relative to calls, pairing, settings, metadata, `OIL COLD`, and low voltage.

## Important Source Locations

| Path | Responsibility |
|---|---|
| `firmware/application/ui/cd53.c` | CD53 modes, buttons, display priority, engine state |
| `firmware/application/ui/cd53.h` | CD53 constants and context |
| `firmware/application/ui/menu/menu_singleline.c` | Shared settings, OBC rendering, device list |
| `firmware/application/ui/menu/menu_singleline.h` | Menu indices, views, oil threshold |
| `firmware/application/lib/ibus.c` | I-Bus decoding, state updates, frame dispatch |
| `firmware/application/lib/config.h` | EEPROM setting addresses and values |
| `firmware/application/upgrade.c` | Versioned default initialization |
| `firmware/application/mappings.h` | Firmware version and hardware mappings |
| `simulator/src/bluebus_sim.c` | Renderer-neutral simulation API and frame generation |
| `simulator/src/tui.c` | Curses UI |
| `simulator/tests/test_simulator.c` | End-to-end host regression checks |
| `simulator/README.md` | Simulator usage and limitations |
| `scripts/build-firmware.sh` | Reproducible macOS production HEX build |

## Commit Timeline

| Commit | Branch/work item | Result |
|---|---|---|
| `8812192` | Device selection | Restored CD53 phone selection and connection behavior |
| `ee14f83` | OBC alerts | Added initial cold-oil/low-voltage display behavior |
| `d0791be` | OBC alerts | Moved low voltage to global display priority |
| `418eb33` | OBC alerts | Required nonzero RPM for the voltage warning |
| `48b0f9d` | OBC alerts | Added settings, defaults, and firmware 1.4.41 migration |
| `e3a6ae3` | OBC alerts | Removed the `activeView` gate and shortened labels |
| `72a78da` | Simulator | Added native TUI, host adapters, production frame path, tests, and CI |
| `20ee6d0` | Simulator CI | Updated checkout action to v7 |
| `10cb509` | Simulator | Reported zero RPM while stopped and added regression checks |
| `d76286b` | Integration | Merged updated OBC alert branch into simulator branch |
| `483e1b8` | Simulator tests | Covered accessible one-screen alert settings |
| `930bfca` | Build tooling | Added tested macOS firmware build script |

## Remaining Validation

The primary outstanding step is controlled testing on real BlueBus hardware in
a vehicle. Specifically verify:

1. `CldOil` and `LowV` are readable and persist after save/restart.
2. A valid oil signal exists on the target vehicle and `OIL COLD` clears at the
   expected warm-up point.
3. Low voltage does not appear with ignition on and engine stopped.
4. Low voltage appears below 13.3 V while RPM is nonzero and overrides every
   other display mode.
5. The normal display returns cleanly at 13.3 V or after engine stop.
6. Button 5 browsing and CD button 2 connection work with two real phones.
7. No unexpected display churn, I-Bus traffic, or Bluetooth reconnect behavior
   occurs during the new alerts.

Until those checks pass, simulator and XC16 results demonstrate software-path
correctness, not complete in-car validation.
