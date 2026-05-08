#include "core/Log.h"

#include <iostream>

namespace cr {

static const char* ToPrefix(LogLevel level) {
  switch (level) {
    case LogLevel::Info: return "[INFO] ";
    case LogLevel::Warn: return "[WARN] ";
    case LogLevel::Error: return "[ERROR]";
  }
  return "[INFO] ";
}

void Log(LogLevel level, std::string_view msg) {
  std::cerr << ToPrefix(level) << msg << "\n";
}

} // namespace cr
