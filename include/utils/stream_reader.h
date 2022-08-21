/*
    stream_reader.h
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Defines a header-only wrapper for std::istream and derived classes
    which provides convenient methods for reading strings and integers
*/

#pragma once

#include <istream>
#include <type_traits>
#include <string>
#include <vector>

namespace d2m {

namespace detail
{
    template<int N>
    struct LoopUnroller
    {
        template<typename Operation>
        inline void operator()(Operation& op) { op(); LoopUnroller<N-1>{}(op); }
    };

    template<>
    struct LoopUnroller<0>
    {
        template<typename Operation>
        inline void operator()(Operation& op) {}
    };
}

enum class Endianness { Unspecified, Little, Big };

/*
    Wrapper for std::istream and derived classes which provides
    convenient methods for reading strings and integers
*/
template <class IStream, Endianness GlobalEndian = Endianness::Unspecified, class = std::enable_if_t<std::is_base_of_v<std::basic_istream<char>, IStream>>>
class StreamReader
{
private:
    IStream m_Stream;

    template <size_t Integer>
    static inline constexpr bool is_power_of_two = ((Integer > 0) && (Integer & (Integer - 1)) == 0);

    template<typename T, size_t Size>
    struct LittleEndianReadOperator
    {
        static constexpr size_t ShiftAmount = (Size - 1) * 8;
        static_assert(Size > 0);

        inline void operator()()
        {
            value >>= 8;
            value |= static_cast<T>(m_Stream.get()) << ShiftAmount;
        }
        IStream& m_Stream;
        T value{};
    };

    template<typename T>
    struct BigEndianReadOperator
    {
        inline void operator()()
        {
            value <<= 8;
            value |= static_cast<T>(m_Stream.get());
        }
        IStream& m_Stream;
        T value{};
    };

    template<typename T, size_t Size, typename = std::enable_if_t<std::is_integral_v<T> && is_power_of_two<sizeof(T)>>>
    inline T ReadIntLittleEndian()
    {
        static_assert(Size <= sizeof(T), "Explicitly specified template parameter 'Size' must be <= sizeof(T)");
        LittleEndianReadOperator<T, Size> oper{stream()};
        detail::LoopUnroller<Size>{}(oper);

        if constexpr (std::is_signed_v<T> && Size < sizeof(T))
        {
            struct SignExtender { T value : Size * 8; };
            return SignExtender{oper.value}.value;
        }
        else
        {
            return oper.value;
        }
    }

    template<typename T, size_t Size, typename = std::enable_if_t<std::is_integral_v<T> && is_power_of_two<sizeof(T)>>>
    inline T ReadIntBigEndian()
    {
        static_assert(Size <= sizeof(T), "Explicitly specified template parameter 'Size' must be <= sizeof(T)");
        BigEndianReadOperator<T> oper{stream()};
        detail::LoopUnroller<Size>{}(oper);

        if constexpr (std::is_signed_v<T> && Size < sizeof(T))
        {
            struct SignExtender { T value : Size * 8; };
            return SignExtender{oper.value}.value;
        }
        else
        {
            return oper.value;
        }
    }

public:

    StreamReader() : m_Stream{} {}

    // StreamReader constructs and owns the istream object
    template <typename... Args>
    StreamReader(Args&&... args) : m_Stream{std::forward<Args>(args)...} {}

    StreamReader(const StreamReader&) = delete;
    StreamReader(StreamReader&&) = delete;
    StreamReader& operator=(const StreamReader&) = delete;
    StreamReader& operator=(StreamReader&&) = delete;

    const IStream& stream() const { return m_Stream; }
    IStream& stream() { return m_Stream; }

    std::string ReadStr(unsigned length)
    {
        std::string tempStr;
        tempStr.assign(length, '\0');
        m_Stream.read(&tempStr[0], length);
        return tempStr;
    }

    std::string ReadPStr()
    {
        // P-Strings (Pascal strings) are prefixed with a 1 byte length
        uint8_t stringLength = m_Stream.get();
        return ReadStr(stringLength);
    }

    std::vector<char> ReadBytes(unsigned length)
    {
        std::vector<char> tempBytes;
        tempBytes.assign(length, '\0');
        m_Stream.read(&tempBytes[0], length);
        return tempBytes;
    }

    template<typename T = uint8_t, size_t ReadAmount = sizeof(T), Endianness Endian = GlobalEndian, typename = std::enable_if_t<std::is_integral_v<T>>>
    T ReadInt()
    {
        static_assert(ReadAmount <= sizeof(T), "Explicitly specified template parameter 'ReadAmount' must be <= sizeof(T)");
        if constexpr (ReadAmount > 1UL)
        {
            static_assert(is_power_of_two<sizeof(T)>, "sizeof(T) must be a power of 2"); // TODO: Unnecessary?
            static_assert(Endian != Endianness::Unspecified, "Set the endianness when creating StreamReader or set it in this method's template parameters");
            if constexpr (Endian == Endianness::Little)
                return ReadIntLittleEndian<T, ReadAmount>();
            else
                return ReadIntBigEndian<T, ReadAmount>();
        }
        else
        {
            return m_Stream.get();
        }
    }
};

} // namespace d2m
