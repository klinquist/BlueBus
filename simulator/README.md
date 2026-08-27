# CD53 Firmware Simulator

This terminal workbench compiles and executes the production BlueBus CD53 UI,
single-line menu, event dispatcher, and I-Bus message handlers. Only the
physical hardware boundary is replaced: EEPROM settings are held in memory,
time is deterministic, Bluetooth commands update simulated state, and I-Bus
transmissions are captured for the display and trace panes.

The workbench does not assign sensor fields directly. Its controls construct
complete checksum-valid BMW I-Bus frames and pass them through
`IBusProcessFrame()`, the same validation and dispatch path used by the UART
loop in firmware.

## Requirements

- CMake 3.20 or newer
- A C99 compiler
- A curses development library
  - macOS: supplied by the command-line developer tools
  - Debian/Ubuntu: `sudo apt install cmake build-essential libncurses-dev`
  - Fedora: `sudo dnf install cmake gcc ncurses-devel`

## Run

From the repository root:

```sh
make -C simulator run
```

Or use CMake directly:

```sh
cmake -S simulator -B simulator/build -DCMAKE_BUILD_TYPE=Debug
cmake --build simulator/build --parallel
./simulator/build/bluebus-cd53-sim
```

The display advances on a 50 ms virtual-time loop. Starting the engine emits a
checksum-valid IKE speed/RPM frame every 100 ms; stopping sends a zero-RPM frame
while keeping the ignition on. Key Off exercises the firmware's normal display
shutdown path.

## Controls

| Key | Action |
|---|---|
| `1`–`6` | Press the corresponding CD button |
| `n` / `p` | Next / previous |
| `e` / `x` / `k` | Start engine / stop engine / key off |
| `r` | Enter RPM |
| `s` | Enter speed in mph |
| `o` / `c` | Enter oil / coolant temperature in Fahrenheit |
| `v` | Enter battery voltage |
| `O` / `V` | Toggle `ColdOilDisp` / `LowVoltWrn` |
| `m` | Enter artist and title metadata |
| `d` | Load two simulated paired phones |
| `a` / `l` | Load the cold-oil / running-low-voltage preset |
| `R` | Reset firmware and simulated hardware state |
| `t` / `q` | Toggle frame trace / quit |

To inspect the paired-device behavior, press `d`, then CD button `5`. The
temporary `Devices` banner clears after its normal firmware timeout, after
which next/previous scroll between the two phone names.

## Tests

```sh
make -C simulator test
```

The regression suite covers:

- I-Bus-driven OBC display updates
- the 160°F cold-oil threshold and setting default
- the 13.3 V threshold, engine-running gate, setting toggle, priority, and recovery
- ignition shutdown and restart
- two-device selection through CD button 5
- generated RX and production TX frame capture

## Architecture and limitations

`src/bluebus_sim.c` is the renderer-neutral public simulator API. The TUI only
calls that API, so another renderer—such as a graphical CD53 faceplate—can be
added without changing the firmware engine.

This is a behavior-level firmware simulator, not a PIC24 instruction emulator.
It intentionally does not model electrical bus arbitration, UART timing, the
BM83/BC127 protocols, audio hardware, or arbitrary raw-frame replay. Bluetooth
commands needed by CD53 menus are represented as deterministic host-side state
changes.
