#pragma once

#include "prisma/command.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <variant>

namespace prisma {

struct RoutedCommand {
    std::size_t worker{};
    Command command;
};

struct ShowHelp {};

struct Quit {};

struct ParseError {
    std::string message;
};

using ConsoleInput = std::variant<RoutedCommand, ShowHelp, Quit, ParseError>;

// Turns one console line into something the main loop can act on.
// Parsing never throws: malformed input becomes a ParseError.
ConsoleInput parseLine(std::string_view line);

std::string_view helpText();

}  // namespace prisma