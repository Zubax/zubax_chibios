ChibiOS libs
============

Build system and libraries for ChibiOS-based embedded software.

Basic usage:

1. Add this repository as git submodule for your project, e.g.: `git submodule add https://github.com/Courierdrone/crdr_chibios firmware/crdr_chibios`
2. Execute `git submodule update --init` from this repository's root
3. Define the ChibiOS configuration headers: `chconf.h` `halconf.h` `mcuconf.h` `board.h`
4. Make sure `chconf.h` ends with `#include <crdr_chibios/sys/chconf_tail.h>`
5. Make sure `halconf.h` ends with `#include <crdr_chibios/sys/halconf_tail.h>`
6. Write the makefile:
    1. List the source files in `CSRC` `CPPSRC`
    2. List the include directories in `UINCDIR`
    3. Define `PROJECT` and `SERIAL_CLI_PORT_NUMBER`
    4. Include `crdr_chibios/rules_<target-mcu>.mk`, e.g. `include crdr_chibios/rules_stm32f105_107.mk`
