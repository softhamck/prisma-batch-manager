#pragma once

#include <mutex>
#include <ostream>
#include <string_view>

namespace prisma {

// Serializes output so lines written by different threads never mix.
class ConsoleLog {
public:
    explicit ConsoleLog(std::ostream& out);

    void write(std::string_view source, std::string_view message);

private:
    std::ostream& out_;
    std::mutex mutex_;
};

}  // namespace prisma
