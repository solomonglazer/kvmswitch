# kvmswitch

A small Windows 11 x64 command test for switching an **LG 38WR85QC-W**
from a Windows PC on DisplayPort to a Mac on USB-C. Requires a directly attached
NVIDIA GPU and its installed driver (the intended setup uses an RTX 4070 SUPER).

**Experimental:** the USB-C command and EDID identities have model-specific Linux/macOS
evidence; this Windows implementation has not been tested on the physical monitor.
An accepted NVAPI write reports `sent`, never `switched`. Verify picture and USB
peripheral migration yourself. No display mode, resolution, refresh rate, HDR,
sleep or USB hub configuration is changed by this program.

## Download and run on your PC

Download the `kvmswitch-windows-x64` artifact from a successful **push** build of
the implementation branch in [GitHub Actions](https://github.com/solomonglazer/kvmswitch/actions).
Extract the complete ZIP into a writable folder and double-click `START-HERE.cmd`.
Choose **1** for read-only readiness checks. Choose **2** when ready for the first
switch; it repeats readiness checks and asks Y/N before sending one command.

No installer or administrator access is required. The Microsoft C++ runtime is
linked into the executable; the installed NVIDIA graphics driver is still required.
Delete the extracted folder to remove the utility. This is an unsigned experimental
test package; if Windows blocks it, report the exact message instead of disabling protection.

`test-results.txt` saves readiness and command output beside the launcher. Copy it
before the next run, which replaces it. It is never sent automatically. Add your
observations about the Mac picture, keyboard/mouse transfer and recovery when
sharing it. `BUILD-INFO.txt` identifies the source commit and build run;
`SHA256SUMS.txt` records package file hashes. See `FIRST-TEST.txt` inside the ZIP.

## Build on Windows

Install Visual Studio 2022 Build Tools with Desktop development with C++ and CMake,
then use an x64 developer terminal:

```powershell
git clone https://github.com/solomonglazer/kvmswitch.git
cd kvmswitch
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

CMake fetches NVIDIA's NVAPI SDK at the pinned revision recorded in
[protocol-evidence](docs/protocol-evidence.md). It links the SDK's x64 import
library; NVAPI runs through the installed NVIDIA driver. No driver or service is
installed by the utility. Review NVIDIA's SDK license before redistributing its
files. The Windows CI workflow compiles and runs mocked tests, verifies package
hashes and smoke-tests the shipped launcher from a folder with spaces. Its smoke
test runs help only and cannot switch a monitor. CI cannot test hardware.

## Run the PC to Mac test

Keep the Mac awake, attached to the monitor's USB-C video port, and ready to display.
Keep the monitor joystick accessible so you can select DisplayPort to recover.
Use the local Windows desktop; remote sessions may not expose display configuration.

```powershell
.\build\Release\kvmswitch.exe list
.\build\Release\kvmswitch.exe doctor
.\build\Release\kvmswitch.exe switch usb-c --dry-run
```

`list` reads active display paths and Windows EDID data. `doctor` and `--dry-run`
also initialize NVAPI, resolve the selected GDI display to one physical GPU/output,
and compare that output's EDID with Windows' EDID. They send no monitor-control writes.
The read-only probe establishes routing and EDID access, **not** successful I2C write capability.

After reviewing the dry-run, run one experimental command:

```powershell
.\build\Release\kvmswitch.exe switch usb-c --confirm-experimental
```

The flag acknowledges the experimental Windows transport. Without it, the utility
refuses to write. Only one allowlisted command can be sent: device `0x6E`, source
`0x50`, payload `84 03 F4 00 D1 9C`, at 33 kHz. There is no raw-value option,
fallback command, retry loop, or automatic switch back. Read the
[hardware test plan](docs/test-plan.md) before testing.

Multiple matching monitors, cloned display paths, other connection types, invalid
EDID checksums, unknown model identities, changed identities, ambiguous GPU routes
and mismatching GPU EDID are refused. There is deliberately no selector override in v0.1.
Only active monitors can be discovered. An inactive input may stop answering after
switching, so use the Mac or joystick to return to Windows.

| Exit | Meaning |
| --- | --- |
| 0 | Listed, probed, dry-run, or write accepted; see status text |
| 2 | Invalid command/flags or missing experimental confirmation |
| 3 | No matching supported model |
| 4 | Multiple matching monitors |
| 5 | Missing/failed NVAPI or GPU/EDID probe |
| 6 | Unsafe or changed identity/route |
| 7 | Write API failure (inspect picture before retrying) |
| 8 | Discovery or unexpected failure |

## Development

The C++17 policy layer is independent of Windows. On Linux/macOS you can compile
and test it with `cmake -S . -B build && cmake --build build && ctest --test-dir build`.
Hardware commands there return exit 5. Mocked tests never link or invoke NVAPI.

The [handoff](docs/HANDOFF.md) is preserved as supplied reference material. It
describes intended behavior, not evidence that this implementation has reached hardware.
The future dashboard companion should wait for a Windows hardware validation before
integrating the switch command; v0.1 requires `--confirm-experimental`.
