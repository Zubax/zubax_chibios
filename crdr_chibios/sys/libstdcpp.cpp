/*
 * Copyright (c) 2014 Courierdrone, courierdrone.com
 * Distributed under the MIT License, available in the file LICENSE.
 * Author: Pavel Kirienko <pavel.kirienko@courierdrone.com>
 */

#include <ch.hpp>
#include <cstdlib>
#include <sys/types.h>
#include "sys.h"

void* operator new(size_t sz)
{
    return chCoreAlloc(sz);
}

void* operator new[](size_t sz)
{
    return chCoreAlloc(sz);
}

void operator delete(void*)
{
    sysPanic("delete");
}

void operator delete[](void*)
{
    sysPanic("delete");
}

/*
 * stdlibc++ workaround.
 * Default implementations will throw, which causes code size explosion.
 * These definitions override the ones defined in the stdlibc+++.
 */
namespace std
{

void __throw_bad_exception() { sysPanic("throw"); }

void __throw_bad_alloc() { sysPanic("throw"); }

void __throw_bad_cast() { sysPanic("throw"); }

void __throw_bad_typeid() { sysPanic("throw"); }

void __throw_logic_error(const char*) { sysPanic("throw"); }

void __throw_domain_error(const char*) { sysPanic("throw"); }

void __throw_invalid_argument(const char*) { sysPanic("throw"); }

void __throw_length_error(const char*) { sysPanic("throw"); }

void __throw_out_of_range(const char*) { sysPanic("throw"); }

void __throw_runtime_error(const char*) { sysPanic("throw"); }

void __throw_range_error(const char*) { sysPanic("throw"); }

void __throw_overflow_error(const char*) { sysPanic("throw"); }

void __throw_underflow_error(const char*) { sysPanic("throw"); }

void __throw_ios_failure(const char*) { sysPanic("throw"); }

void __throw_system_error(int) { sysPanic("throw"); }

void __throw_future_error(int) { sysPanic("throw"); }

void __throw_bad_function_call() { sysPanic("throw"); }

}

namespace __gnu_cxx
{

void __verbose_terminate_handler()
{
    sysPanic("terminate");
}

}

extern "C"
{

int __aeabi_atexit(void*, void(*)(void*), void*)
{
    return 0;
}

__extension__ typedef int __guard __attribute__((mode (__DI__)));

void __cxa_atexit(void(*)(void *), void*, void*)
{
}

int __cxa_guard_acquire(__guard* g)
{
    return !*g;
}

void __cxa_guard_release (__guard* g)
{
    *g = 1;
}

void __cxa_guard_abort (__guard*)
{
}

void __cxa_pure_virtual()
{
    sysPanic("pure virtual");
}

}

