/*
 * Copyright (c) 2016 Zubax, zubax.com
 * Distributed under the MIT License, available in the file LICENSE.
 * Author: Pavel Kirienko <pavel.kirienko@zubax.com>
 */

/*
 * Collection of heap-less containers and functions.
 */

#pragma once

#include <type_traits>
#include <algorithm>
#include <cassert>
#include <cstdarg>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <cctype>


namespace os
{
namespace heapless
{
/**
 * Converts any signed or unsigned integer or boolean to string and returns it by value.
 * The argument must be of integral type, otherwise the call will be rejected by SFINAE.
 * Usage examples:
 *      intToString(var)
 *      intToString<16>(var)
 *      intToString<2>(var).c_str()
 * It is safe to obtain a reference to the returned string and pass it to another function as an argument,
 * which enables use cases like this (this example is somewhat made up, but it conveys the idea nicely):
 *      print("%s", intToString(123).c_str());
 * More info on rvalue references:
 *      https://herbsutter.com/2008/01/01/gotw-88-a-candidate-for-the-most-important-const/
 *      http://stackoverflow.com/questions/584824/guaranteed-lifetime-of-temporary-in-c
 */
template <
    int Radix = 10,
    typename T,
    typename std::enable_if<std::is_integral<T>::value>::type...
    >
inline auto intToString(T number)
{
    constexpr char Alphabet[] = "0123456789abcdefghijklmnopqrstuvwxyz";

    static_assert(Radix >= 1, "Radix must be positive");
    static_assert(Radix <= (sizeof(Alphabet) / sizeof(Alphabet[0])), "Radix is too large");

    // Plus 1 to round up, see the standard for details.
    constexpr unsigned MaxChars =
        ((Radix >= 10) ? std::numeric_limits<T>::digits10 : std::numeric_limits<T>::digits) +
        1 + (std::is_signed<T>::value ? 1 : 0);

    class Container
    {
        std::uint_fast16_t offset_;
        char storage_[MaxChars + 1];   // Plus 1 because of zero termination.

    public:
        Container(T x) :
            offset_(MaxChars)          // Field initialization is not working in GCC in this context, not sure why.
        {
            const bool negative = std::is_signed<T>::value && (x < 0);

            storage_[offset_] = '\0';

            do
            {
                assert(offset_ > 0);

                if (std::is_signed<T>::value)  // Should be converted to constexpr if.
                {
                    // We can't just do "x = -x", because it would break on int8_t(-128), int16_t(-32768), etc.
                    auto residual = std::int_fast8_t(x % Radix);
                    if (residual < 0)
                    {
                        // Should never happen - since C++11, neg % pos --> pos
                        residual = -residual;
                    }

                    storage_[--offset_] = Alphabet[residual];

                    // Signed integers are really tricky sometimes.
                    // We must not mix negative with positive to avoid implementation-defined behaviors.
                    x = (x < 0) ? -(x / -Radix) : (x / Radix);
                }
                else
                {
                    // Fast branch for unsigned arguments.
                    storage_[--offset_] = Alphabet[x % Radix];
                    x /= Radix;
                }
            }
            while (x != 0);

            if (negative)    // Should be optimized with constexpr if.
            {
                assert(offset_ > 0);
                storage_[--offset_] = '-';
            }

            assert(offset_ < MaxChars);                 // Making sure there was no overflow.
        }

        const char* c_str() const { return &storage_[offset_]; }

        operator const char* () const { return this->c_str(); }
    };

    return Container(number);
}

/**
 * The default capacity is optimal for most embedded use cases.
 */
constexpr unsigned DefaultStringCapacity = 200;

/**
 * Heapless string that keeps all data inside a fixed length buffer.
 * The capacity of the buffer can be specified via template arguments.
 * The interface is similar to that of std::string and other standard containers.
 */
template <unsigned Capacity_ = DefaultStringCapacity>
class String
{
    static_assert(Capacity_ > 0, "Capacity must be positive");

    template <unsigned C>
    friend class String;

public:
    static constexpr unsigned Capacity = Capacity_;

private:
    unsigned len_ = 0;
    char buf_[Capacity + 1] = {};

public:
    constexpr String() { }

    String(const char* const initializer) { append(initializer); }

    template <unsigned C>
    String(const String<C>& initializer) { append(initializer); }

    /*
     * Formatting
     */
    template <typename... Args>
    auto format(Args... format_args) const
    {
        String<(DefaultStringCapacity > Capacity) ? DefaultStringCapacity : Capacity> output;

        using namespace std;
        const int res = snprintf(output.begin(), output.capacity(), this->c_str(), format_args...);
        if (res > 0)
        {
            output.len_ = std::min(output.capacity(), unsigned(res));
        }

        return output;
    }

    /*
     * std::string API
     */
    using value_type = char;
    using size_type = unsigned;
    using iterator = char*;
    using const_iterator = const iterator;

    constexpr unsigned capacity() const { return Capacity; }
    constexpr unsigned max_size() const { return Capacity; }

    unsigned size()   const { return len_; }
    unsigned length() const { return len_; }

    bool empty() const { return len_ == 0; }

    const char* c_str() const { return &buf_[0]; }

    void clear() { len_ = 0; }

    template <unsigned C>
    void append(const String<C>& s)
    {
        append(s.c_str());
    }

    void append(const char* p)
    {
        if (p != nullptr)
        {
            while ((*p != '\0') && (len_ < Capacity))
            {
                buf_[len_++] = *p++;
            }
            buf_[len_] = '\0';
            assert(buf_[Capacity] == '\0');
        }
    }

    void append(char p)
    {
        if (len_ < Capacity)
        {
            buf_[len_++] = p;
        }
        buf_[len_] = '\0';
        assert(buf_[Capacity] == '\0');
    }

    template <typename T>
    typename std::enable_if<std::is_integral<T>::value>::type append(const T& value)
    {
        append(intToString(value).c_str());
    }

    template <typename T>
    typename std::enable_if<std::is_floating_point<T>::value>::type append(const T& value)
    {
        constexpr int Precision = std::numeric_limits<T>::digits10 + 1;
        static_assert(Precision > 1, "Invalid floating point type?");

        char buffer[20];

        using namespace std;
        (void) snprintf(buffer, sizeof(buffer), "%.*g", Precision, double(value));

        append(static_cast<const char*>(&buffer[0]));
    }

    void push_back(char c) { append(c); }

    template <typename T>
    void concatenate(const T& head)
    {
        this->append(head);
    }

    template <typename T, typename... Args>
    void concatenate(const T& head, Args... tail)
    {
        this->append(head);
        this->concatenate(tail...);
    }

    char&       front()       { return operator[](0); }
    const char& front() const { return operator[](0); }

    char& back()
    {
        if (len_ > 0)
        {
            return buf_[len_ - 1U];
        }
        else
        {
            assert(false);
            return buf_[0];
        }
    }
    const char& back() const { return const_cast<String*>(this)->back(); }

    template <unsigned C>
    bool compare(const String<C>& s) const
    {
        return compare(s.c_str());
    }

    bool compare(const char* s) const
    {
        if (s != nullptr)
        {
            return 0 == std::strncmp(this->c_str(), s, Capacity);
        }
        else
        {
            return false;
        }
    }

    /*
     * Iterators
     */
    const char* begin() const { return &buf_[0]; }
    const char* end()   const { return &buf_[len_]; }

    char* begin() { return &buf_[0]; }
    char* end()   { return &buf_[len_]; }

    /*
     * Operators
     */
    template <typename T>
    String& operator=(const T& s)
    {
        clear();
        append(s);
        return *this;
    }

    template <typename T>
    String& operator+=(const T& s)
    {
        append(s);
        return *this;
    }

    char& operator[](unsigned index)
    {
        if (index < len_)
        {
            return buf_[index];
        }
        else
        {
            assert(false);
            return back();
        }
    }
    const char& operator[](unsigned index) const { return const_cast<String*>(this)->operator[](index); }

    template <typename T>
    bool operator==(const T& s) const { return compare(s); }

    /*
     * Helpers
     */
    String<Capacity> toLowerCase() const
    {
        String<Capacity> out;
        std::transform(begin(), end(), std::back_inserter(out), [](char c) { return char(std::tolower(c)); });
        return out;
    }

    String<Capacity> toUpperCase() const
    {
        String<Capacity> out;
        std::transform(begin(), end(), std::back_inserter(out), [](char c) { return char(std::toupper(c)); });
        return out;
    }

    template <typename Left, typename Right>
    static auto join(const Left& left, const Right& right)
    {
        String<(DefaultStringCapacity > Capacity) ? DefaultStringCapacity : Capacity> output(left);
        output.append(right);
        return output;
    }

    struct Formatter
    {
        String<Capacity> format_string;

        Formatter(const char* fmt) : format_string(fmt) { }

        template <typename... Args>
        auto operator()(Args... format_args)
        {
            return format_string.format(format_args...);
        }
    };
};

/**
 * Operators for String<>.
 * @{
 */
template <unsigned LeftCapacity, unsigned RightCapacity>
inline auto operator+(const String<LeftCapacity>& left, const String<RightCapacity>& right)
{
    return String<LeftCapacity + RightCapacity>::join(left, right);
}

template <unsigned Capacity>
inline auto operator+(const String<Capacity>& left, const char* right)
{
    return String<Capacity>::join(left, right);
}

template <unsigned Capacity>
inline auto operator+(const char* left, const String<Capacity>& right)
{
    return String<Capacity>::join(left, right);
}

template <unsigned Capacity>
inline bool operator==(const char* left, const String<Capacity>& right)
{
    return right.compare(left);
}
/**
 * @}
 */

/**
 * Converts a string literal into String<>.
 * Usage example:
 *      auto len = "Hello world!"_heapless.length();
 */
inline auto operator "" _heapless(const char* str, std::size_t)
{
    return String<>(str);
}

/**
 * Formats a string literal using heapless string.
 * Usage example:
 *      auto str = "The %s answer is %d!"_format("Great", 42);
 */
inline auto operator "" _format(const char* str, std::size_t)
{
    return String<>::Formatter(str);
}

/**
 * Formats an arbitrary string and returns it as a heapless string instance.
 * Usage example:
 *      conat auto str = format("The %s answer is %d!", "Great", 42);
 */
template<unsigned FormatLength, typename... Args>
inline auto format(const char (&format_string)[FormatLength], Args... format_args)
{
    return String<FormatLength>(format_string).format(format_args...);
}

/**
 * Like Python's print(), except that it returns the string by value as a heapless instance instead of
 * printing it.
 * Usage example:
 *      auto str = concatenate("The Great Answer is", 42, "!\n");
 */
template <unsigned Capacity = DefaultStringCapacity, typename... Args>
inline auto concatenate(Args... arguments)
{
    String<Capacity> s;
    s.concatenate(arguments...);
    return s;
}

/**
 * Compatibility with standard streams, nothing special here.
 */
template <typename Stream, unsigned Capacity>
inline Stream& operator<<(Stream& stream, const String<Capacity>& obj)
{
    stream << obj.c_str();
    return stream;
}

}
}
