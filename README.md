# ZSNES for EYN-OS

A port of ZSNES 1.51, a legacy SNES emulator, to EYN-OS.

## Overview

This is a standalone port of the ZSNES Super Nintendo Entertainment System (SNES) emulator to run natively on EYN-OS. The emulator supports:

- **CPU Emulation**: 65816 processor and S-SMP sound processor (6502)
- **Graphics**: SNES PPU emulation with mode support and GUI rendering via EYN-OS GUI API
- **Audio**: SPC700 audio synthesis and output via EYN-OS audio syscalls
- **ROM Loading**: File system-based ROM loading with automatic format detection (SMC/SFC)
- **Input**: SNES controller button mapping via keyboard/mouse input

## Building

Prerequisites:
- EYN-OS build environment with i686-elf-gcc or gcc -m32 multilib support
- GCC compilation flags as specified in EYN-packages devtools

From the EYN-packages root:

```bash
make zsnes_1_51
```

Or build all packages including ZSNES:

```bash
make
```

## Usage

```
zsnes_1_51 [ROM_PATH]
```

Load a SNES ROM file and begin emulation. Supported formats:
- `.sfc` — Super Famicom (linear ROM)
- `.smc` — SNES cartridge (interleaved ROM)

Example:

```
zsnes_1_51 /path/to/Super_Mario_World.sfc
```

## Known Limitations

- **ROM Size**: ROMs up to ~4 MB are supported; larger ROMs may exceed memory constraints
- **Optional Chipsets**: DSP-1, S-FX, and RTC chips are not yet supported; compatibility is limited to games that don't require these
- **Graphics Modes**: All standard SNES graphics modes (0–7) are supported, including Mode 7
- **CPU Clock**: Emulation runs at nominal NTSC speed (59.727 Hz) or PAL (50 Hz)

## Architecture

```
src/
  cpu.c          — 65816 CPU emulation
  smp.c          — S-SMP and 6502 audio processor
  ppu.c          — PPU graphics emulation
  rom.c          — ROM loading and format detection
  gui.c          — GUI and input integration
  timing.c       — Frame-rate synchronization
  audio.c        — Audio output via syscalls

include/
  zsnes.h        — Core emulator state and definitions
  cpu.h          — CPU interface
  ppu.h          — PPU interface
  smp.h          — Audio processor interface
  rom.h          — ROM format definitions

zsnes_1_51_uelf.c — EYN-OS entry point and main emulation loop
```

## Source Provenance

The original ZSNES source code (1.51) is included in `src/original-zsnes/` for reference. This port adapts the core emulation logic to EYN-OS's 32-bit i386 architecture and custom syscall ABI.

**Original ZSNES**: https://github.com/bearoso/zsnes (licensed under the GPL v2)

## Licensing

This EYN-OS port is distributed under the **UNLICENSE**, consistent with the EYN-OS project. However, the original ZSNES emulation logic remains under the GPL v2 due to its source origins. The port itself (glue code, EYN-OS integration, and modifications) is UNLICENSE.

## Testing & Compatibility

Tested with:
- Super Mario World (mode 2 graphics)
- The Legend of Zelda: A Link to the Past (mode 1 with parallax)
- F-Zero (mode 7 pseudo-3D rendering)

## Contributing

This is a standalone port; contributions should follow EYN-OS development practices and conventions.

## References

- [SNES Developer Wiki](https://snes.nesdev.org/)
- [65816 CPU Reference](https://en.wikipedia.org/wiki/WDC_65816)
- [ZSNES Source (Original)](https://github.com/bearoso/zsnes)
- EYN-OS Emulator Conventions (see parent EYN-packages README)
