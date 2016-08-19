#pragma once

#include <cmath>
#include <limits>

namespace ant {
namespace std_ext {

constexpr double inf = std::numeric_limits<double>::infinity();
constexpr double NaN = std::numeric_limits<double>::quiet_NaN();

template <typename T>
constexpr inline T degree_to_radian(const T& degree) noexcept {
  return degree * M_PI / 180.0;
}

template <typename T>
constexpr inline T radian_to_degree(const T& radian) noexcept {
  return radian * 180.0 / M_PI;
}

template <typename T>
constexpr inline T sqr(const T& x) noexcept { return x*x; }

template<typename T>
constexpr inline T abs_diff(T a, T b) noexcept {
  return a > b ? a - b : b - a;
}

template<class T>
struct RMS_t {
    unsigned n = 0;
    T sum{};
    T sum2{};
    void Add(const T& v) {
        ++n;
        sum += v;
        sum2 += std_ext::sqr(v);
    }
    T GetMean() const {
        return sum/n;
    }
    T GetRMS() const {
        return std::sqrt(std::abs(sum2/n - std_ext::sqr(GetMean())));
    }
};

using RMS = RMS_t<double>;

}} // namespace ant::std_ext

