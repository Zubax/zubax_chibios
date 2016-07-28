/*
 * Copyright (c) 2014 Zubax, zubax.com
 * Distributed under the MIT License, available in the file LICENSE.
 * Author: Pavel Kirienko <pavel.kirienko@zubax.com>
 */

#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <zubax_chibios/os.hpp>

namespace os
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
            int len = int(std::strlen(nm));
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
        std::printf("%-*s = %-12f", _max_name_len, name, double(configGet(name)));
        if (verbose)
        {
            std::printf("[%f, %f] (%f)", double(par.min), double(par.max), double(par.default_));
        }
    }
    else
    {
        std::printf("%-*s = %-12i", _max_name_len, name, (int)configGet(name));
        if (verbose)
        {
            std::printf("[%i, %i] (%i)", (int)par.min, (int)par.max, (int)par.default_);
        }
    }
    std::puts("");
    return 0;
}

int executeCLICommand(int argc, char *argv[])
{
    const char* const command = (argc < 1) ? "" : argv[0];

    if (!std::strcmp(command, "list"))
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
    else if (!std::strcmp(command, "save"))
    {
        return configSave();
    }
    else if (!std::strcmp(command, "erase"))
    {
        return configErase();
    }
    else if (!std::strcmp(command, "get"))
    {
        if (argc < 2)
        {
            std::puts("Error: Not enough arguments");
            return -EINVAL;
        }
        return printParam(argv[1], false);
    }
    else if (!std::strcmp(command, "set"))
    {
        if (argc < 3)
        {
            std::puts("Error: Not enough arguments");
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
        std::puts("Usage:\n"
                  "  cfg list\n"
                  "  cfg save\n"
                  "  cfg erase\n"
                  "  cfg get <name>\n"
                  "  cfg set <name> <value>\n"
                  "Note that save or erase may halt CPU for a few milliseconds which\n"
                  "may cause transient failures in real time tasks or communications.");
    }
    return -EINVAL;
}

}
}
