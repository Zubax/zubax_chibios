/*
 * Copyright (c) 2014 Zubax, zubax.com
 * Distributed under the MIT License, available in the file LICENSE.
 * Author: Pavel Kirienko <pavel.kirienko@zubax.com>
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <zubax_chibios/config/cli.hpp>
#include <zubax_chibios/config/config.hpp>

namespace zubax_chibios
{
namespace config
{

static int printParam(const char* name, bool verbose)
{
    static int _max_name_len;
    if (_max_name_len == 0)
    {
        for (int i = 0;; i++)
        {
            const char* nm = configNameByIndex(i);
            if (!nm)
            {
                break;
            }
            int len = int(strlen(nm));
            if (len > _max_name_len)
            {
                _max_name_len = len;
            }
        }
    }

    ConfigParam par;
    const int res = configGetDescr(name, &par);
    if (res)
    {
        return res;
    }

    if (par.type == CONFIG_TYPE_FLOAT)
    {
        printf("%-*s = %-12f", _max_name_len, name, configGet(name));
        if (verbose)
        {
            printf("[%f, %f] (%f)", par.min, par.max, par.default_);
        }
    }
    else
    {
        printf("%-*s = %-12i", _max_name_len, name, (int)configGet(name));
        if (verbose)
        {
            printf("[%i, %i] (%i)", (int)par.min, (int)par.max, (int)par.default_);
        }
    }
    puts("");
    return 0;
}

int executeCliCommand(int argc, char *argv[])
{
    const char* const command = (argc < 1) ? "" : argv[0];

    if (!strcmp(command, "list"))
    {
        for (int i = 0;; i++)
        {
            const char* name = configNameByIndex(i);
            if (!name)
            {
                break;
            }
            const int res = printParam(name, true);
            if (res)
            {
                return res;
            }
        }
        return 0;
    }
    else if (!strcmp(command, "save"))
    {
        return configSave();
    }
    else if (!strcmp(command, "erase"))
    {
        return configErase();
    }
    else if (!strcmp(command, "get"))
    {
        if (argc < 2)
        {
            puts("Error: Not enough arguments");
            return -EINVAL;
        }
        return printParam(argv[1], false);
    }
    else if (!strcmp(command, "set"))
    {
        if (argc < 3)
        {
            puts("Error: Not enough arguments");
            return -EINVAL;
        }
        const char* const name = argv[1];
        const float value = atoff(argv[2]);
        int res = configSet(name, value);
        if (res == 0)
        {
            res = printParam(name, false);
        }
        return res;
    }
    else
    {
        puts("Usage:\n"
            "  cfg list\n"
            "  cfg save\n"
            "  cfg erase\n"
            "  cfg get <name>\n"
            "  cfg set <name> <value>\n"
            "Note that save or erase may halt CPU for a few milliseconds which\n"
            "can cause transient failures in real time tasks or communications.");
    }
    return -EINVAL;
}

}
}
