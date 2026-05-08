#pragma once

#include <string_view>

namespace cr {

enum class LogLevel {
  Info,
  Warn,
  Error,
};

void Log(LogLevel level, std::string_view msg);

} // namespace cr
