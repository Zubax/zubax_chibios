#
# Copyright (c) 2014 Zubax, zubax.com
# Distributed under the MIT License, available in the file LICENSE.
# Author: Pavel Kirienko <pavel.kirienko@zubax.com>
#

ZUBAX_CHIBIOS_DIR := $(dir $(lastword $(MAKEFILE_LIST)))

CPPSRC += $(ZUBAX_CHIBIOS_DIR)/zubax_chibios/platform/stm32/sys_stm32.cpp               \
          $(ZUBAX_CHIBIOS_DIR)/zubax_chibios/platform/stm32/watchdog_stm32.cpp          \

CHIBIOS := $(ZUBAX_CHIBIOS_DIR)/chibios
include $(CHIBIOS)/os/common/ports/ARMCMx/compilers/GCC/mk/startup_stm32f3xx.mk
include $(CHIBIOS)/os/hal/ports/STM32/STM32F37x/platform.mk
include $(CHIBIOS)/os/rt/ports/ARMCMx/compilers/GCC/mk/port_v7m.mk

ifeq ($(LDSCRIPT),)
    $(error Default LDSCRIPT is not available for this platform; the application must provide a custom linker script.)
endif

USE_FPU = hard
MCU = cortex-m4

include $(ZUBAX_CHIBIOS_DIR)/_rules_armcm.mk
