/*
 * Copyright (c) 2014 Courierdrone, courierdrone.com
 * Distributed under the MIT License, available in the file LICENSE.
 * Author: Pavel Kirienko <pavel.kirienko@courierdrone.com>
 */

#pragma once

namespace crdr_chibios
{
namespace config
{

/**
 * Allows to read/modify/save/restore commands via CLI
 */
int executeCliCommand(int argc, char *argv[]);

}
}
