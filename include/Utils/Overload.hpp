#pragma once

namespace Utils {

template <typename... Ts>
struct Overload : Ts... {
  using Ts::operator()...;
};

}  // namespace Utils
