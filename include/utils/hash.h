/*
 * hash.h
 * Written by Dalton Messmer <messmer.dalton@gmail.com>.
 *
 * Custom hash functions
 */

#pragma once

#include <cstddef>
#include <functional>
#include <utility>

struct PairHash
{
	template<class T1, class T2>
	auto operator()(const std::pair<T1, T2>& pair) const noexcept -> std::size_t
	{
		return std::hash<T1>{}(pair.first) ^ std::hash<T2>{}(pair.second);
	}
};
