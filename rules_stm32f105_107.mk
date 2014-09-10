#
# Copyright (c) 2014 Zubax, zubax.com
# Distributed under the MIT License, available in the file LICENSE.
# Author: Pavel Kirienko <pavel.kirienko@zubax.com>
#

ZUBAX_CHIBIOS_DIR := $(dir $(lastword $(MAKEFILE_LIST)))

CSRC += $(ZUBAX_CHIBIOS_DIR)/zubax_chibios/config/config.c                      \
        $(ZUBAX_CHIBIOS_DIR)/zubax_chibios/config/config_storage_stm32.c        \
        $(ZUBAX_CHIBIOS_DIR)/zubax_chibios/watchdog/watchdog_stm32.c            \
        $(ZUBAX_CHIBIOS_DIR)/zubax_chibios/sys/sys_stm32.c

CPPSRC += $(ZUBAX_CHIBIOS_DIR)/zubax_chibios/config/config_cli.cpp

CHIBIOS := $(ZUBAX_CHIBIOS_DIR)/chibios
include $(CHIBIOS)/os/hal/platforms/STM32F1xx/platform_f105_f107.mk
include $(CHIBIOS)/os/ports/GCC/ARMCMx/STM32F1xx/port.mk

LDSCRIPT ?= $(PORTLD)/STM32F107xC.ld

include $(ZUBAX_CHIBIOS_DIR)/_rules_common.mk
