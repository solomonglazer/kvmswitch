# kvmswitch — Windows KVM Switching Utility

**Status:** Development handoff / specification, 2026-10-29. No repository created, code compiled, or monitor commands executed as part of this handoff.

## 1. Mission
Build a minimal, safe Windows 11 command-line utility named `kvmswitch` that switches the **LG 38WR85QC-W** from Windows **DisplayPort** video to macOS **USB-C** video using the monitor's vendor-specific electronic monitor-control mechanism. The LG's existing hardware USB KVM is expected to migrate peripherals when the video input changes, but the Windows-driven direction remains **untested**.

Project context: this program will eventually be called by the Desk Dashboard Windows companion application after a dedicated physical button sends a USB control event through the dashboard's macro-controller firmware. The USB hub itself needs no software.

## 2. Evidence and confirmed behavior
- Setup: LG 38WR85QC-W (3840×1600, 144 Hz), PC RTX 4070 SUPER connected over DP and M3 Pro MacBook over monitor USB-C.
- **User-confirmed:** Running `monmux` on macOS switched LG video from Mac USB-C to Windows DP automatically; mouse/keyboard and other USB peripheral control migrated to Windows, just like joystick-based switching. No monitor-menu action required.
- **Not established:** Windows-originating electronic input switching to USB-C, repeatability across sleep/wake, monitor read-back of actual selected input, exact confirmed LG input code and transport encoding for Windows.
- Monmux: https://github.com/leinardi/monmux . Its implementation invokes `m1ddc` on macOS and protects writes with monitor identification and an evidence-backed input allowlist. Check `docs/compatibility.md`, `docs/backends.md` and source when implementing; do **not** assume values for winddc are interchangeable without verifying the framing.
- Windows reference: https://github.com/choplin/winddc (MIT licensed per uploaded LICENSE/README). Uses Windows Dxva2 to enumerate monitors, NVIDIA NVAPI raw I2C for alternative LG input control. See uploaded `main.cpp`, `nvapi_helper.cpp`, `vcp.*` and `monitor_store.*`.

## 3. Scope
### v0.1
1. List/identify LG monitors without changing anything.
2. Confirm NVIDIA GPU and DDC communication capability.
3. Show the exact intended monitor, command, and target input with `--dry-run` (no write).
4. `switch usb-c` sends **only** a verified LG command and returns a status of *sent*, not *monitor switched*, unless independently verified.
5. Failure messages and exit codes distinguish unsupported model, ambiguous monitor, missing NVAPI, invalid configuration and attempted write failure.
6. Support manual recovery with monitor joystick.

### Later
- Optional `switch dp`, standardized status output for the Desk Dashboard companion, device events, cooldown/debounce, sleep/wake tests, robust monitor selection, packaging and code-signing considerations.

### Explicit non-goals
- No arbitrary VCP writes or command-value arguments; no changing Windows display modes, suppressing video, putting computers to sleep, KVM over network, telemetry, startup service, GUI or USB hub management.
- No purchases, installation or changes to the user’s computer without approval.

## 4. Proposed architecture
`CLI parser → monitor discovery + EDID/model validation → target allowlist → NVIDIA NVAPI I2C write → result reporting`.

Use a slim C++17 application with CMake. Prefer Microsoft's supported monitor enumeration/EDID paths for the identity check and NVAPI only for the vendor-specific write. A real EDID match should include manufacturer/model data; reject ambiguous monitors rather than defaulting to display #1. Protect against different device-to-GPU mappings and verify output routing.

A future dashboard call is simply `kvmswitch switch usb-c`. The dashboard button triggers a **USB event** to the PC companion; the companion invokes the utility or an embedded library. Do not run a shell script on the microcontroller. The command should complete before the monitor transfers USB control and the companion loses its link to the panel.

## 5. Review of provided winddc sources: actionable findings
- `utils.cpp::narrowString`: allocates `required - 1` bytes then supplies `required` to `WideCharToMultiByte`, a size mismatch and memory-safety defect. Fix, test and do not reuse the buggy code.
- `main.cpp`: allows generic user-supplied numeric values for `input-alt`; no EDID allowlist; defaults to first monitor. Replace with model-verified, whitelisted targets.
- `nvapi_helper.cpp`: builds an LG-specific 0xF4 write on NVIDIA's I2C interface with source address handling; exact validated target values **must be independently established** for this LG.
- `utils.cpp::parseLong`: consider input range and `errno` handling for robust parsing, but avoid numeric raw-value interfaces entirely in v0.1.
- `nvapi_helper.cpp`: check GPU/monitor association carefully, especially for multi-output PCs; first GPU returned may not always be correct.
- `CMAKELISTS.txt`: links `Dxva2` and vendored NVIDIA `nvapi64.lib` from a submodule; audit exact library and build dependencies before trust.
- `ci.yml` and `release.yml`: automate builds/releases using third-party actions; pin dependencies for reproducible releases. A source review does **not** authenticate a downloaded .exe.
- No obvious network exfiltration, auto-start persistence or credential handling identified in the uploaded program source. This is **not** a malware certification or binary audit.

## 6. Safety gates
1. Review and pin source revisions and dependencies, including NVAPI submodule.
2. Compile with warnings, static analysis, and Windows runtime checks where possible.
3. Unit-test command validation and monitor allowlisting with mocked write backend; hardware writes disabled in tests.
4. Run read-only monitor discovery on Windows and verify correct LG identification.
5. Dry-run target command and verify bytes, device identity, GPU route against known-tested evidence.
6. Only with explicit user approval, send one validated Windows→USB-C command while Mac and PC are on; keep monitor joystick accessible.
7. Record picture change, USB peripheral migration, utility exit output and recovery; repeat later under controlled conditions.

**Do not assume `winddc.exe` is safe simply because uploaded source appears plausible.** Prefer a locally built, reviewed executable and verify release provenance if using a distributed binary.

## 7. Suggested project repository
```
kvmswitch/
  README.md
  docs/
    HANDOFF.md
    test-plan.md
    protocol-evidence.md
    security-notes.md
  src/
    main.cpp
    monitor_discovery.cpp
    monitor_discovery.h
    nvapi_backend.cpp
    nvapi_backend.h
    lg_commands.cpp
    lg_commands.h
  tests/
    command_validation_tests.cpp
  CMakeLists.txt
  LICENSE
```
*Repository contents are a proposal, not yet created on GitHub.*

## 8. Acceptance criteria
- The tool refuses absent or ambiguous matching LG monitors and never sends an undocumented value.
- Dry-run and read-only commands cause zero DDC writes.
- On the real PC: `kvmswitch switch usb-c` changes LG video to Mac USB-C and automatically migrates monitor-attached USB devices; no joystick interaction required.
- Incorrect model, disconnected monitor or unsupported NVAPI fail safely.
- No change to PC resolution, refresh rate or HDR configuration.
- Physical joystick can restore picture if switching fails.

## 9. Follow-on Desk Dashboard integration
Windows companion listens for a distinct `KVM_SWITCH` HID/vendor event, calls `kvmswitch switch usb-c`, handles result appropriately, and then loses USB control as the monitor transfers the hub. macOS companion uses monmux (or later a shared native backend) to switch to DP. Display state confirmation is a separate problem; a successful API write is not reliable read-back. Add cooldown, reconnection and resilience to wake/sleep. Keep monitor video lines direct to LG.

## 10. Next action for repo owner
Create GitHub repo `kvmswitch`; put this document at `docs/HANDOFF.md`; start with read-only monitor discovery and Windows NVAPI feasibility. Do not port a generic raw-write CLI or run arbitrary VCP commands.
