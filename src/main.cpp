#include "prisma/command_parser.hpp"
#include "prisma/worker.hpp"

#include <array>
#include <iostream>
#include <string>
#include <variant>

int main() {
    using namespace prisma;

    BatchRepository repository;
    ConsoleLog log(std::cout);
    log.write("main", helpText());

    {
        Worker first("T1", repository, log);
        Worker second("T2", repository, log);
        const std::array<Worker*, 2> workers{&first, &second};

        // The main thread only reads and routes commands; the workers do the
        // actual work, so typing never waits for a command to finish.
        std::string line;
        while (std::getline(std::cin, line)) {
            if (line.find_first_not_of(" \t\r") == std::string::npos) {
                continue;
            }

            const ConsoleInput input = parseLine(line);
            if (const auto* routed = std::get_if<RoutedCommand>(&input)) {
                workers[routed->worker]->submit(routed->command);
            } else if (const auto* error = std::get_if<ParseError>(&input)) {
                log.write("main", error->message);
            } else if (std::holds_alternative<ShowHelp>(input)) {
                log.write("main", helpText());
            } else {
                break;
            }
        }

        log.write("main", "Finishing pending commands...");
    }  // workers drain their queues and join here

    log.write("main", "Bye");
    return 0;
}