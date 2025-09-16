#pragma once

#include <type_traits>

template <class T>
class Flags
{
public:
    using MaskType = std::underlying_type_t<T>;

    constexpr          Flags(                ) noexcept : mMask(0) {}
    constexpr          Flags(T bit           ) noexcept : mMask(static_cast<MaskType>(bit)) {}
    constexpr          Flags(const Flags& rhs) noexcept = default;
    constexpr explicit Flags(MaskType flags  ) noexcept : mMask(flags) {}

    // Relational Operators
    auto operator<=>(const Flags&) const = default;

    // Logical Operator
    constexpr bool operator!() const noexcept
    {
        return !mMask;
    }
    
    // Bitwise Operators
    constexpr Flags operator&(const Flags& rhs) const noexcept
    {
        return Flags(mMask & rhs.mMask);
    }

    constexpr Flags operator|(const Flags& rhs) const noexcept
    {
        return Flags(mMask | rhs.mMask);
    }

    constexpr Flags operator^(const Flags& rhs) const noexcept
    {
        return Flags(mMask ^ rhs.mMask);
    }

    // Assignment Operators
    constexpr Flags& operator=(const Flags& rhs) noexcept = default;

    constexpr Flags& operator&=(const Flags& rhs) noexcept
    {
        mMask &= rhs.mMask;
        return *this;
    }

    constexpr Flags& operator|=(const Flags& rhs) noexcept
    {
        mMask |= rhs.mMask;
        return *this;
    }

    constexpr Flags& operator^=(const Flags& rhs) noexcept
    {
        mMask ^= rhs.mMask;
        return *this;
    }

    // Cast Operators
    explicit constexpr operator bool() const noexcept
    {
        return !!mMask;
    }

    explicit constexpr operator MaskType() const noexcept
    {
        return mMask;
    }

private:
    MaskType mMask;
};

// Bitwise Operators : Flags
template <class T>
constexpr Flags<T> operator&(T t, const Flags<T>& flags) noexcept
{
    return flags.operator&(t);
}

template <class T>
constexpr Flags<T> operator|(T t, const Flags<T>& flags) noexcept
{
    return flags.operator|(t);
}

template <class T>
constexpr Flags<T> operator^(T t, const Flags<T>& flags) noexcept
{
    return flags.operator^(t);
}
