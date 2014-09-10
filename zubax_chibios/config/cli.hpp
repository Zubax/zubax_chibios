/*
 * Copyright (c) 2014 Zubax, zubax.com
 * Distributed under the MIT License, available in the file LICENSE.
 * Author: Pavel Kirienko <pavel.kirienko@zubax.com>
 */

#pragma once

namespace zubax_chibios
{
namespace config
{

/**
 * Allows to read/modify/save/restore commands via CLI
 */
int executeCliCommand(int argc, char *argv[]);

}
}
