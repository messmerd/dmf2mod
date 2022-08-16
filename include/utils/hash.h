/*
    hash.h
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Custom hash functions
*/

#pragma once

#include <cstddef>
#include <utility>
#include <functional>

struct PairHash
{
    template<class T1, class T2>
    size_t operator()(std::pair<T1, T2> const &pair) const
    {
        return std::hash<T1>()(pair.first) ^ std::hash<T2>()(pair.second);
    }
};
