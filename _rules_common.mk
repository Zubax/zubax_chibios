#
# Copyright (c) 2014 Courierdrone, courierdrone.com
# Distributed under the MIT License, available in the file LICENSE.
# Author: Pavel Kirienko <pavel.kirienko@courierdrone.com>
#

CRDR_CHIBIOS_DIR := $(dir $(lastword $(MAKEFILE_LIST)))

CSRC += 

UINCDIR += $(CRDR_CHIBIOS_DIR)

UDEFS +=

#
# OS configuration
#

ifndef SERIAL_CLI_PORT_NUMBER
    $(error SERIAL_CLI_PORT_NUMBER must be assigned an integer value greater than zero)
endif

UDEFS += -DSTDOUT_SD=SD$(SERIAL_CLI_PORT_NUMBER) -DSTDIN_SD=STDOUT_SD -DSERIAL_CLI_PORT_NUMBER=$(SERIAL_CLI_PORT_NUMBER)

UDEFS += -DCORTEX_ENABLE_WFI_IDLE=1 -DCHPRINTF_USE_FLOAT=1

USE_LINK_GC = yes
USE_THUMB ?= yes
USE_VERBOSE_COMPILE ?= no
USE_FWLIB ?= no

CHIBIOS := $(CRDR_CHIBIOS_DIR)/chibios
include $(CHIBIOS)/os/hal/hal.mk
include $(CHIBIOS)/os/kernel/kernel.mk
include $(CHIBIOS)/os/various/cpp_wrappers/kernel.mk


VARIOUSSRC = $(CHIBIOS)/os/various/syscalls.c $(CHIBIOS)/os/various/chprintf.c $(CHIBIOS)/os/various/shell.c

CSRC += $(PORTSRC) $(KERNSRC) $(HALSRC) $(PLATFORMSRC) $(VARIOUSSRC)

CPPSRC += $(CHCPPSRC)

ASMSRC += $(PORTASM)

INCDIR += $(PORTINC) $(KERNINC) $(HALINC) $(PLATFORMINC) $(CHCPPINC) $(CHIBIOS)/os/various

#
# Build configuration
#

NO_BUILTIN += -fno-builtin-printf -fno-builtin-fprintf  -fno-builtin-vprintf -fno-builtin-vfprintf -fno-builtin-puts

USE_OPT += -falign-functions=16 -U__STRICT_ANSI__ $(NO_BUILTIN)
USE_COPT += -std=c99
USE_CPPOPT += -std=c++11 -fno-rtti -fno-exceptions

RELEASE ?= 0
RELEASE_OPT ?= -O1 -fomit-frame-pointer
DEBUG_OPT ?= -O1 -g3
ifneq ($(RELEASE),0)
    DDEFS += -DRELEASE
    USE_OPT += $(RELEASE_OPT)
else
    DDEFS += -DDEBUG
    USE_OPT += $(DEBUG_OPT)
endif

#
# Compiler options
#

MCU  = cortex-m3

TOOLCHAIN_PREFIX ?= arm-none-eabi
CC   = $(TOOLCHAIN_PREFIX)-gcc
CPPC = $(TOOLCHAIN_PREFIX)-g++
LD   = $(TOOLCHAIN_PREFIX)-g++
CP   = $(TOOLCHAIN_PREFIX)-objcopy
AS   = $(TOOLCHAIN_PREFIX)-gcc -x assembler-with-cpp
OD   = $(TOOLCHAIN_PREFIX)-objdump
HEX  = $(CP) -O ihex
BIN  = $(CP) -O binary

# ARM-specific options here
AOPT +=

# THUMB-specific options here
TOPT ?= -mthumb -DTHUMB=1

CWARN += -Wall -Wextra -Werror -Wstrict-prototypes
CPPWARN += -Wall -Wextra -Werror

# asm statement fix
DDEFS += -Dasm=__asm

include $(CHIBIOS)/os/ports/GCC/ARMCMx/rules.mk
