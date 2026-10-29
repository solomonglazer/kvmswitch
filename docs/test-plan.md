# Windows hardware test

No hardware command has been run by development in the cloud environment.

1. Build with Visual Studio x64; run CTest. Confirm the executable came from the
   reviewed source and the pinned NVAPI SDK.
2. With Windows displaying over DP and the Mac awake on USB-C, make sure the joystick
   can restore DP. Save work before handing monitor-attached keyboard/mouse to the Mac.
3. Run `list`. Confirm the intended LG identity (normally GSM/77D3 on DP), external
   DisplayPort and a unique route. Connect only one matching monitor. Run `doctor`.
4. Run `switch usb-c --dry-run`. Confirm the GDI display, EDID route match, NVIDIA
   output mask and bytes `84 03 F4 00 D1 9C` with source `50` and device `6E`.
5. When ready, run `switch usb-c --confirm-experimental` once. There are no retries.
   `sent` means only NVAPI accepted the write. A write error does not prove nothing
   happened; inspect the picture before another attempt.
6. Record exit code/output, driver and firmware versions, whether the Mac picture
   appeared, and whether keyboard, mouse and other USB devices migrated. Use the Mac's
   existing working monmux command or joystick to restore DP. Record recovery.
7. Only after a successful first test, separately test repeatability and sleep/wake.
   Compare resolution, refresh rate and HDR settings before/after. Do not infer these
   results from an API return code.

Suggested result record (do not commit serial numbers):

```text
Date / source commit:
Windows / NVIDIA driver / LG firmware:
list / doctor / dry-run output:
Switch exit code and output:
Mac picture appeared: yes/no
USB keyboard / mouse / other peripherals migrated:
Recovery method and outcome:
Windows display settings before / after:
Observations:
```
