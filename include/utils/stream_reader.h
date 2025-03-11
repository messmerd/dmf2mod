/*
 * stream_reader.h
 * Written by Dalton Messmer <messmer.dalton@gmail.com>.
 *
 * Defines a header-only wrapper for std::istream and derived classes
 * which provides convenient methods for reading strings and integers
 */

#pragma once

#include <istream>
#include <type_traits>
#include <cstdint>
#include <string>
#include <vector>

namespace d2m {

namespace detail {
	template<int times>
	struct LoopUnroller
	{
		template<typename Operation>
		inline void operator()(Operation& op) { op(); LoopUnroller<times - 1>{}(op); }
	};

	template<>
	struct LoopUnroller<0>
	{
		template<typename Operation>
		inline void operator()(Operation& op) {}
	};

	// Adapted from: https://peter.bloomfield.online/using-cpp-templates-for-size-based-type-selection/
	template<uint8_t num_bytes>
	using UIntSelector =
		typename std::conditional<num_bytes == 1, uint_fast8_t,
			typename std::conditional<num_bytes == 2, uint_fast16_t,
				typename std::conditional<num_bytes == 3 || num_bytes == 4, uint_fast32_t,
					uint_fast64_t
				>::type
			>::type
		>::type;
} // namespace detail

enum class Endianness { kUnspecified, kLittle, kBig };

/*
 * Wrapper for std::istream and derived classes which provides
 * convenient methods for reading strings and integers
 */
template<class IStream, Endianness global_endian = Endianness::kUnspecified, std::enable_if_t<std::is_base_of_v<std::basic_istream<char>, IStream>, bool> = true>
class StreamReader
{
private:
	IStream stream_;

	template<typename T, uint8_t num_bytes>
	struct LittleEndianReadOperator
	{
		static constexpr uint_fast8_t kShiftAmount = (num_bytes - 1) * 8;
		static_assert(num_bytes > 0);

		inline void operator()()
		{
			value >>= 8;
			value |= static_cast<T>(stream_.get()) << kShiftAmount;
		}
		IStream& stream_;
		T value{};
	};

	template<typename T>
	struct BigEndianReadOperator
	{
		inline void operator()()
		{
			value <<= 8;
			value |= static_cast<T>(stream_.get());
		}
		IStream& stream_;
		T value{};
	};

	template<typename T, bool is_signed, uint8_t num_bytes>
	inline auto ReadIntLittleEndian() -> T
	{
		LittleEndianReadOperator<T, num_bytes> oper{stream()};
		detail::LoopUnroller<num_bytes>{}(oper);

		if constexpr (is_signed && num_bytes < sizeof(T))
		{
			struct SignExtender { T value : num_bytes * 8; };
			return SignExtender{oper.value}.value;
		}
		else
		{
			return oper.value;
		}
	}

	template<typename T, bool is_signed, uint8_t num_bytes>
	inline auto ReadIntBigEndian() -> T
	{
		BigEndianReadOperator<T> oper{stream()};
		detail::LoopUnroller<num_bytes>{}(oper);

		if constexpr (is_signed && num_bytes < sizeof(T))
		{
			struct SignExtender { T value : num_bytes * 8; };
			return SignExtender{oper.value}.value;
		}
		else
		{
			return oper.value;
		}
	}

public:

	StreamReader() = default;

	// StreamReader constructs and owns the istream object
	template<typename... Args>
	StreamReader(Args&&... args) : stream_{std::forward<Args>(args)...} {}

	StreamReader(const StreamReader&) = delete;
	StreamReader(StreamReader&&) = delete;
	auto operator=(const StreamReader&) -> StreamReader& = delete;
	auto operator=(StreamReader&&) -> StreamReader& = delete;

	auto stream() const -> const IStream& { return stream_; }
	auto stream() -> IStream& { return stream_; }

	auto ReadStr(unsigned length) -> std::string
	{
		std::string temp_str;
		temp_str.assign(length, '\0');
		stream_.read(&temp_str[0], length);
		return temp_str;
	}

	auto ReadPStr() -> std::string
	{
		// P-Strings (Pascal strings) are prefixed with a 1 byte length
		uint8_t string_length = stream_.get();
		return ReadStr(string_length);
	}

	auto ReadBytes(unsigned length) -> std::vector<char>
	{
		std::vector<char> temp_bytes;
		temp_bytes.assign(length, '\0');
		stream_.read(&temp_bytes[0], length);
		return temp_bytes;
	}

	template<bool is_signed = false, uint8_t num_bytes = 1, Endianness endian = global_endian>
	auto ReadInt()
	{
		using UIntType = std::conditional_t<(num_bytes > 1), detail::UIntSelector<num_bytes>, uint8_t>;
		using ReturnType = std::conditional_t<is_signed, std::make_signed_t<UIntType>, UIntType>;

		static_assert(num_bytes <= 8 && num_bytes >= 1, "Accepted range for num_bytes: 1 <= num_bytes <= 8");
		if constexpr (num_bytes > 1)
		{
			static_assert(endian != Endianness::kUnspecified, "Set the endianness when creating StreamReader or set it in this method's template parameters");
			if constexpr (endian == Endianness::kLittle)
			{
				return static_cast<ReturnType>(ReadIntLittleEndian<UIntType, is_signed, num_bytes>());
			}
			else
			{
				return static_cast<ReturnType>(ReadIntBigEndian<UIntType, is_signed, num_bytes>());
			}
		}
		else
		{
			// For single-byte reads, the size of the return value is guaranteed
			// to be 1 byte and setting the signed parameter is unnecessary
			return static_cast<ReturnType>(stream_.get());
		}
	}
};

} // namespace d2m
