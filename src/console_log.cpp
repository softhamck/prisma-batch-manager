#include "prisma/console_log.hpp"

#include <string>

namespace prisma {

ConsoleLog::ConsoleLog(std::ostream& out) : out_(out) {}

void ConsoleLog::write(std::string_view source, std::string_view message) {
    std::string line;
    line.reserve(source.size() + message.size() + 4);
    line.append("[").append(source).append("] ").append(message).append("\n");

    std::lock_guard lock(mutex_);
    out_ << line << std::flush;
}

}  // namespace prisma