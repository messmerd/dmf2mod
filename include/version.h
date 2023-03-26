/*
 * version.h
 * Written by Dalton Messmer <messmer.dalton@gmail.com>.
 *
 * dmf2mod version info
 */

#pragma once

#include <string_view>

namespace d2m {

inline constexpr int kVersionMajor = 0;
inline constexpr int kVersionMinor = 2;
inline constexpr int kVersionPatch = 0;
inline constexpr std::string_view kVersion = "0.2.0-alpha";

} // namespace d2m
