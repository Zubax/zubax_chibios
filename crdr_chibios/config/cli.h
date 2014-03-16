/*
 * Copyright (c) 2014 Courierdrone, courierdrone.com
 * Distributed under the MIT License, available in the file LICENSE.
 * Author: Pavel Kirienko <pavel.kirienko@courierdrone.com>
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Allows to read/modify/save/restore commands via CLI
 */
int configExecuteCliCommand(int argc, char *argv[]);

#ifdef __cplusplus
}
#endif
