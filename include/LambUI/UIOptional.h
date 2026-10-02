#pragma once

// Boost is a soft dependency: bring your own via LAMBUI_USE_BOOST if you
// want boost::optional (e.g. to stay on C++14) or already depend on Boost
// elsewhere. Otherwise this falls back to std::optional, which requires the
// library and its consumers to compile as C++17 or newer.
#if defined(LAMBUI_USE_BOOST)
#include <boost/optional.hpp>
namespace LambUI {
template <typename T>
using Optional = boost::optional<T>;
}
#else
#include <optional>
namespace LambUI {
template <typename T>
using Optional = std::optional<T>;
}
#endif
