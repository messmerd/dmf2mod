/*
    stream_reader.h
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Defines a header-only wrapper for std::istream and derived classes
    which provides convenient methods for reading strings and integers
*/

#pragma once

#include <istream>
#include <type_traits>
#include <cstdint>
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

    // Adapted from: https://peter.bloomfield.online/using-cpp-templates-for-size-based-type-selection/
    template <uint8_t NumBytes>
    using UIntSelector =
        typename std::conditional<NumBytes == 1, uint_fast8_t,
            typename std::conditional<NumBytes == 2, uint_fast16_t,
                typename std::conditional<NumBytes == 3 || NumBytes == 4, uint_fast32_t,
                    uint_fast64_t
                >::type
            >::type
        >::type;
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

    template<typename T, uint8_t NumBytes>
    struct LittleEndianReadOperator
    {
        static constexpr uint_fast8_t ShiftAmount = (NumBytes - 1) * 8;
        static_assert(NumBytes > 0);

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

    template<typename T, bool Signed, uint8_t NumBytes>
    inline T ReadIntLittleEndian()
    {
        LittleEndianReadOperator<T, NumBytes> oper{stream()};
        detail::LoopUnroller<NumBytes>{}(oper);

        if constexpr (Signed && NumBytes < sizeof(T))
        {
            struct SignExtender { T value : NumBytes * 8; };
            return SignExtender{oper.value}.value;
        }
        else
        {
            return oper.value;
        }
    }

    template<typename T, bool Signed, uint8_t NumBytes>
    inline T ReadIntBigEndian()
    {
        BigEndianReadOperator<T> oper{stream()};
        detail::LoopUnroller<NumBytes>{}(oper);

        if constexpr (Signed && NumBytes < sizeof(T))
        {
            struct SignExtender { T value : NumBytes * 8; };
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

    template<bool Signed = false, uint8_t NumBytes = 1, Endianness Endian = GlobalEndian>
    auto ReadInt()
    {
        using UIntType = std::conditional_t<(NumBytes > 1), detail::UIntSelector<NumBytes>, uint8_t>;
        using ReturnType = std::conditional_t<Signed, std::make_signed_t<UIntType>, UIntType>;

        static_assert(NumBytes <= 8 && NumBytes >= 1, "Accepted range for NumBytes: 1 <= NumBytes <= 8");
        if constexpr (NumBytes > 1)
        {
            static_assert(Endian != Endianness::Unspecified, "Set the endianness when creating StreamReader or set it in this method's template parameters");
            if constexpr (Endian == Endianness::Little)
                return static_cast<ReturnType>(ReadIntLittleEndian<UIntType, Signed, NumBytes>());
            else
                return static_cast<ReturnType>(ReadIntBigEndian<UIntType, Signed, NumBytes>());
        }
        else
        {
            // For single-byte reads, the size of the return value is guaranteed
            // to be 1 byte and setting the Signed parameter is unnecessary
            return static_cast<ReturnType>(m_Stream.get());
        }
    }
};

} // namespace d2m
