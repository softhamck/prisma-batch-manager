#include "prisma/command_parser.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <optional>
#include <sstream>
#include <system_error>
#include <vector>

namespace prisma {

namespace {

std::string toLower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

std::vector<std::string> split(const std::string& line) {
    std::istringstream stream(line);
    std::vector<std::string> tokens;
    for (std::string token; stream >> token;) {
        tokens.push_back(token);
    }
    return tokens;
}

// std::from_chars is locale independent, never throws and lets us reject
// trailing garbage such as "12abc".
template <typename Number>
std::optional<Number> parseNumber(const std::string& text) {
    Number value{};
    const char* last = text.data() + text.size();
    const auto [end, error] = std::from_chars(text.data(), last, value);
    if (error != std::errc{} || end != last) {
        return std::nullopt;
    }
    return value;
}

std::optional<std::size_t> parseWorker(const std::string& token) {
    if (token == "t1") {
        return 0;
    }
    if (token == "t2") {
        return 1;
    }
    return std::nullopt;
}

ParseError usage(std::string_view syntax) {
    return ParseError{"Usage: <t1|t2> " + std::string(syntax)};
}

ParseError invalid(std::string_view what, const std::string& token) {
    return ParseError{"Invalid " + std::string(what) + ": " + token};
}

// tokens[0] is the worker, tokens[1] the command, the rest its arguments.
ConsoleInput parseCommand(std::size_t worker, const std::vector<std::string>& tokens) {
    const std::string& name = tokens[1];
    const std::size_t argCount = tokens.size() - 2;

    if (name == "create") {
        if (argCount != 1) {
            return usage("create <initial_kg>");
        }
        const auto weight = parseNumber<double>(tokens[2]);
        if (!weight) {
            return invalid("weight", tokens[2]);
        }
        return RoutedCommand{worker, CreateBatch{*weight}};
    }

    if (name == "get" || name == "shrink") {
        if (argCount != 1) {
            return usage(name + " <id>");
        }
        const auto id = parseNumber<BatchId>(tokens[2]);
        if (!id) {
            return invalid("id", tokens[2]);
        }
        if (name == "get") {
            return RoutedCommand{worker, GetBatch{*id}};
        }
        return RoutedCommand{worker, GetShrinkage{*id}};
    }

    if (name == "state") {
        if (argCount != 2) {
            return usage("state <id> <roasting|cooling|cancelled>");
        }
        const auto id = parseNumber<BatchId>(tokens[2]);
        if (!id) {
            return invalid("id", tokens[2]);
        }
        const auto state = parseState(tokens[3]);
        if (!state) {
            return invalid("state", tokens[3]);
        }
        return RoutedCommand{worker, ChangeState{*id, *state}};
    }

    if (name == "finish") {
        if (argCount != 2) {
            return usage("finish <id> <final_kg>");
        }
        const auto id = parseNumber<BatchId>(tokens[2]);
        if (!id) {
            return invalid("id", tokens[2]);
        }
        const auto weight = parseNumber<double>(tokens[3]);
        if (!weight) {
            return invalid("weight", tokens[3]);
        }
        return RoutedCommand{worker, FinishBatch{*id, *weight}};
    }

    if (name == "finished") {
        if (argCount != 0) {
            return usage("finished");
        }
        return RoutedCommand{worker, ListFinished{}};
    }

    return ParseError{"Unknown command: " + name + " (type help)"};
}

}  // namespace

ConsoleInput parseLine(std::string_view line) {
    const auto tokens = split(toLower(std::string(line)));

    if (tokens.empty()) {
        return ParseError{"Empty command"};
    }
    if (tokens.size() == 1 && tokens[0] == "help") {
        return ShowHelp{};
    }
    if (tokens.size() == 1 && tokens[0] == "exit") {
        return Quit{};
    }

    const auto worker = parseWorker(tokens[0]);
    if (!worker) {
        return ParseError{"Unknown target: " + tokens[0] + " (use t1 or t2, or type help)"};
    }
    if (tokens.size() < 2) {
        return ParseError{"Missing command after " + tokens[0]};
    }
    return parseCommand(*worker, tokens);
}

std::string_view helpText() {
    return R"(Commands (send each one to thread t1 or t2):
  <t1|t2> create <initial_kg>                     create a batch
  <t1|t2> get <id>                                show a batch
  <t1|t2> state <id> <roasting|cooling|cancelled> change the state
  <t1|t2> finish <id> <final_kg>                  finish a batch (needs final weight)
  <t1|t2> finished                                list finished batches
  <t1|t2> shrink <id>                             shrinkage percentage
  help                                            show this help
  exit                                            wait for pending work and quit)";
}

}  // namespace prisma