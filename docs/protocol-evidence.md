# Command evidence and transport

Reviewed 2026-10-29. These are source observations, not a Windows hardware result.

1. [monmux catalog at c27b940](https://github.com/leinardi/monmux/blob/c27b940c01bd37c606709cb966a581adb3fa0bd6/internal/catalog/models.yaml)
   records LG 38WR85QC-W identities `GSM/0x77D3` (DP) and `GSM/0x77D4` (USB-C).
   Its USB-C input is `0xD1`, directly tested on this model with Linux ddcutil and
   macOS m1ddc on 2026-09-07. DP is `0xD0`, deliberately not exposed by this v0.1 test.
2. [monmux backend documentation at the same revision](https://github.com/leinardi/monmux/blob/c27b940c01bd37c606709cb966a581adb3fa0bd6/docs/backends.md)
   identifies the LG alternate mechanism as VCP `0xF4`, DDC/CI source address
   `0x50`. It passes `0xD1` as a 16-bit value (high byte zero), not `0xD100`.
   USB-C `210` from generic m1ddc help is not the tested value for this model.
3. [winddc NVAPI framing at 9d897ea](https://github.com/choplin/winddc/blob/9d897ea5ecd314b99fd0757664e1ccb50efd448e/src/nvapi_helper.cpp)
   uses NVAPI device address `0x6E` (8-bit DDC address; 7-bit `0x37`), a one-byte
   register/source field and six payload bytes:
   `84 03 <VCP> <value-high> <value-low> <checksum>`. The checksum is XOR of
   `6E`, source, and the first five payload bytes. For this command it is `9C`.
   Source `50` is not the I2C slave address. The packet construction here is
   implemented independently from this documented protocol; no winddc sources are vendored.
4. The official [NVIDIA NVAPI SDK](https://github.com/NVIDIA/nvapi/tree/3d34a4faf095996663321646ebe003539a908f89)
   is pinned at `3d34a4faf095996663321646ebe003539a908f89`, also the winddc submodule
   revision. No SDK binary is downloaded or executed during Linux core tests.

Windows discovery uses QueryDisplayConfig active paths, then SetupAPI monitor
interface matching and the monitor device's registry EDID. The base EDID must have
its standard header, checksum and matching manufacturer/product. The GDI source
must serve exactly one active target and be external DisplayPort. NVAPI resolves
that GDI source to exactly one GPU and single-bit output mask; that output's
EDID must match the complete 128-byte Windows base EDID. This avoids choosing the
first enumerated GPU. The identity and route are checked again before the write.

Remaining uncertainty: the Windows/NVAPI framing has not been tested on this LG;
DP AUX tunneling, driver/firmware behavior and USB migration remain unproven.
Registry EDID and NVAPI EDID can be cached. Identical EDIDs do not distinguish
units without unique serials. This program does not authenticate monitors, and
cannot eliminate the final hardware hotplug race. It refuses cases it can detect.
No VCP read-back is used as proof of input state.
