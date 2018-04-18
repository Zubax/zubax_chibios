/*
 * Copyright (c) 2014-2017 Zubax, zubax.com
 * Distributed under the MIT License, available in the file LICENSE.
 * Author: Pavel Kirienko <pavel.kirienko@zubax.com>
 */

#include <ch.hpp>
#include <cstdlib>
#include <sys/types.h>
#include "sys.hpp"
#include <new>


static std::uint8_t g_operator_new_returns_pointer_to_this;

[[noreturn]]
static void _libstdcpp_panic(const char* msg)
{
    chSysHalt(msg);
    for (;;) { }
}


void* operator new(std::size_t)
{
    _libstdcpp_panic("operator new()");
    return &g_operator_new_returns_pointer_to_this;
}

void* operator new[](std::size_t)
{
    _libstdcpp_panic("operator new[]()");
    return &g_operator_new_returns_pointer_to_this;
}

void operator delete(void*)
{
    _libstdcpp_panic("delete");
}

void operator delete[](void*)
{
    _libstdcpp_panic("delete");
}

void operator delete(void*, std::size_t)
{
    _libstdcpp_panic("delete");
}

void operator delete[](void*, std::size_t)
{
    _libstdcpp_panic("delete");
}

#if __cpp_aligned_new
void operator delete(void*, std::align_val_t)
{
    _libstdcpp_panic("delete");
}

void operator delete[](void*, std::align_val_t)
{
    _libstdcpp_panic("delete");
}

void operator delete(void*, std::size_t, std::align_val_t)
{
    _libstdcpp_panic("delete");
}

void operator delete[](void*, std::size_t, std::align_val_t)
{
    _libstdcpp_panic("delete");
}
#endif

/*
 * stdlibc++ workaround.
 * Default implementations will throw, which causes code size explosion.
 * These definitions override the ones defined in the stdlibc+++.
 */
namespace std
{

void __throw_bad_exception() { _libstdcpp_panic("throw"); }

void __throw_bad_alloc() { _libstdcpp_panic("throw"); }

void __throw_bad_cast() { _libstdcpp_panic("throw"); }

void __throw_bad_typeid() { _libstdcpp_panic("throw"); }

void __throw_logic_error(const char*) { _libstdcpp_panic("throw"); }

void __throw_domain_error(const char*) { _libstdcpp_panic("throw"); }

void __throw_invalid_argument(const char*) { _libstdcpp_panic("throw"); }

void __throw_length_error(const char*) { _libstdcpp_panic("throw"); }

void __throw_out_of_range(const char*) { _libstdcpp_panic("throw"); }

void __throw_out_of_range_fmt(const char*, ...) { _libstdcpp_panic("throw"); }

void __throw_runtime_error(const char*) { _libstdcpp_panic("throw"); }

void __throw_range_error(const char*) { _libstdcpp_panic("throw"); }

void __throw_overflow_error(const char*) { _libstdcpp_panic("throw"); }

void __throw_underflow_error(const char*) { _libstdcpp_panic("throw"); }

void __throw_ios_failure(const char*) { _libstdcpp_panic("throw"); }

void __throw_system_error(int) { _libstdcpp_panic("throw"); }

void __throw_future_error(int) { _libstdcpp_panic("throw"); }

void __throw_bad_function_call() { _libstdcpp_panic("throw"); }

}

namespace __gnu_cxx
{

void __verbose_terminate_handler()
{
    _libstdcpp_panic("terminate");
}

}

extern "C"
{

int __aeabi_atexit(void*, void(*)(void*), void*)
{
    return 0;
}

#ifdef __arm__
/**
 * Ref. "Run-time ABI for the ARM Architecture" page 23..24
 * http://infocenter.arm.com/help/topic/com.arm.doc.ihi0043d/IHI0043D_rtabi.pdf
 *
 * ChibiOS issue: http://forum.chibios.org/phpbb/viewtopic.php?f=3&t=2404
 *
 * A 32-bit, 4-byte-aligned static data value. The least significant 2 bits
 * must be statically initialized to 0.
 */
typedef int __guard;
#else
# error "Unknown architecture"
#endif

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

}

