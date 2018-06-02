ChibiOS libs
============

[![Join the chat at https://gitter.im/Zubax/general](https://img.shields.io/badge/GITTER-join%20chat-green.svg)](https://gitter.im/Zubax/general)

-----

**WARNING: NOT FOR NEW PROJECTS!**

**This repository has been refactored into a set of separate, independent libraries:**
* <https://github.com/Zubax/senoval>
* <https://github.com/Zubax/legilimens>
* <https://github.com/Zubax/kocherga>
* Some of the functionality has been merged into the ChibiOS upstream: <http://www.chibios.com/forum/viewtopic.php?f=35&t=4613>

If your project is dependent on this repository, please refactor it to include ChibiOS directly,
and feel free to make use of the replacement libraries instead.

-----

Build system and libraries for ChibiOS-based embedded software.

Basic usage:

1. Add this repository as git submodule for your project, e.g.: `git submodule add https://github.com/Zubax/zubax_chibios firmware/zubax_chibios`
2. Execute `git submodule update --init` from this repository's root
3. Define the ChibiOS configuration headers: `chconf.h` `halconf.h` `mcuconf.h` `board.h`
4. Write the makefile:
    1. List the source files in `CSRC` `CPPSRC`
    2. List the include directories in `UINCDIR`
    3. Define `PROJECT` and `SERIAL_CLI_PORT_NUMBER`
    4. Include `zubax_chibios/rules_<target-mcu>.mk`, e.g. `include zubax_chibios/rules_stm32f105_107.mk`

## Debugging using Eclipse and Black Magic Debug Probe

- Open your Eclipse project
- Go Eclipse → Window → Preferences → Run/Debug → Launching → Default Launchers:
  - Select `GDB Hardware Debugging` → `[Debug]`, then tick *only* `Legacy GDB Hardware Debugging Launcher`, and make sure that the option for GDB (DSF) is disabled.
- Go Eclipse → Run → Debug Configurations:
  - Invoke the context menu for `GDB Hardware Debugging`, select New.
  - Tab `Debugger`:
    - Set the field `GDB Command` to `arm-none-eabi-gdb` (or other if necessary).
    - Untick `Use remote target`.
  - Tab `Startup`:
    - If a boot loader is used, make sure that `Image offset` is configured correctly.
    - Enter the following in the field `Initialization Commands`:
```gdb
target extended <BLACK_MAGIC_SERIAL_PORT>
monitor swdp_scan   # Use jtag_scan instead if necessary
attach 1
```
