#pragma once

namespace LambUI {

// Replaces boost::algorithm::clamp for the library's internal implementation
// files now that Boost is a soft, bring-your-own dependency (see
// UIOptional.h); matches its (value, lo, hi) argument order.
template <typename T>
constexpr T Clamp(const T& value, const T& lo, const T& hi) {
    return value < lo ? lo : (hi < value ? hi : value);
}

} // namespace LambUI
