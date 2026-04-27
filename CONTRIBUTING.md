# Contributing to ZSNES for EYN-OS

## Source Provenance

This port of ZSNES to EYN-OS is based on ZSNES 1.51, the last stable release of the ZSNES emulator before the project stalled.

**Original ZSNES Project**:
- Repository: https://github.com/bearoso/zsnes
- License: GPL v2
- Author: bearoso and contributors

The original ZSNES codebase is written primarily in x86 assembly and C, targeting legacy Linux and DOS platforms. This EYN-OS port adapts the core emulation logic (CPU, PPU, SMP) to run on EYN-OS's i386 32-bit architecture with its custom syscall ABI.

## Licensing

This EYN-OS port is distributed under the **UNLICENSE**, consistent with the EYN-OS project. However:

- **Original emulation logic**: GPL v2 (derived from ZSNES)
- **EYN-OS integration code**: UNLICENSE (wrapper, GUI/audio syscall hooks, build system)

The GPL v2 license applies to the core emulation algorithms and instruction set implementations. The UNLICENSE applies to the glue code and modifications for EYN-OS compatibility.

## Port Structure

The port is organized as follows:

```
zsnes_1_51/
├── zsnes_uelf.c              (EYN-OS entry point, UNLICENSE)
├── src/
│   ├── cpu.c                 (CPU emulation, GPL v2 origin)
│   ├── ppu.c                 (Graphics, GPL v2 origin)
│   ├── smp.c                 (Audio, GPL v2 origin)
│   ├── rom.c                 (ROM loading, UNLICENSE port-specific)
│   └── original-zsnes/       (Original ZSNES source for reference, GPL v2)
└── include/
    ├── zsnes.h               (Core definitions, UNLICENSE)
    ├── cpu.h                 (CPU interface, UNLICENSE)
    ├── ppu.h                 (PPU interface, UNLICENSE)
    ├── smp.h                 (SMP interface, UNLICENSE)
    └── rom.h                 (ROM loading, UNLICENSE)
```

## Modifications for EYN-OS

Key modifications from the original ZSNES:

1. **CPU & PPU refactoring**: Extracted instruction-set and graphics logic into portable C functions, removing x86 ASM dependencies
2. **Memory mapping**: Adapted SNES memory addressing to EYN-OS's 32-bit i386 protected mode
3. **GUI integration**: Replaced SDL/X11 graphics with EYN-OS GUI syscalls
4. **Audio output**: Replaced ALSA/OSS with EYN-OS audio syscalls
5. **Input handling**: Mapped keyboard input to SNES controller buttons via EYN-OS GUI events
6. **Build system**: Created EYN-packages-compatible build wrapper

## Acknowledgments

- **ZSNES developers**: For the original emulator and comprehensive SNES emulation reference
- **EYN-OS community**: For the EYN-OS platform and developer tools

## GPL Compliance

This EYN-OS port complies with the GPL v2 license:

- The original ZSNES source code is included in `src/original-zsnes/` for reference and modification
- All modifications to GPL-licensed code are documented
- The source code is freely available
- The UNLICENSE does not override the GPL v2 for derived works

## Contributing Guidelines

When contributing to this port:

1. **GPL-covered code**: Changes to emulation logic (CPU, PPU, SMP) are covered by GPL v2
2. **Port-specific code**: EYN-OS integration (syscall wrappers, build glue) can be UNLICENSE
3. **Attribution**: Document which original ZSNES code your changes are based on
4. **Testing**: Verify compatibility with known SNES games before submitting

## License Questions

For questions about the licensing, consult:
- ZSNES GPL v2 license terms (original repository)
- EYN-OS UNLICENSE (parent project)
- SNES development documentation (neutral reference)
