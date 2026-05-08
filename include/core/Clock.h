#pragma once

#include <cstdint>

namespace cr {

class Clock {
public:
  void Reset();
  double TickSeconds();

private:
  std::uint64_t m_last = 0;
  double m_freq = 0.0;
};

} // namespace cr
