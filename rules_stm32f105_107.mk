#
# Copyright (c) 2014 Courierdrone, courierdrone.com
# Distributed under the MIT License, available in the file LICENSE.
# Author: Pavel Kirienko <pavel.kirienko@courierdrone.com>
#

CRDR_CHIBIOS_DIR := $(dir $(lastword $(MAKEFILE_LIST)))

CSRC += $(CRDR_CHIBIOS_DIR)/crdr_chibios/config/config.c                      \
        $(CRDR_CHIBIOS_DIR)/crdr_chibios/config/config_storage_stm32.c        \
        $(CRDR_CHIBIOS_DIR)/crdr_chibios/watchdog/watchdog_stm32.c            \
        $(CRDR_CHIBIOS_DIR)/crdr_chibios/sys/sys.c                            \
        $(CRDR_CHIBIOS_DIR)/crdr_chibios/sys/sys_stm32.c

CHIBIOS := $(CRDR_CHIBIOS_DIR)/chibios
include $(CHIBIOS)/os/hal/platforms/STM32F1xx/platform_f105_f107.mk
include $(CHIBIOS)/os/ports/GCC/ARMCMx/STM32F1xx/port.mk

LDSCRIPT ?= $(PORTLD)/STM32F107xC.ld

include $(CRDR_CHIBIOS_DIR)/_rules_common.mk
