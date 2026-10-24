#pragma once

#include "prisma/batch_repository.hpp"
#include "prisma/blocking_queue.hpp"
#include "prisma/command.hpp"
#include "prisma/console_log.hpp"

#include <string>
#include <thread>

namespace prisma {

// Owns one thread that executes commands, in arrival order, against the
// shared repository.
class Worker {
public:
    Worker(std::string name, BatchRepository& repository, ConsoleLog& log);
    ~Worker();

    Worker(const Worker&) = delete;
    Worker& operator=(const Worker&) = delete;

    bool submit(Command command);
    void stop();

private:
    void run();

    void handle(const CreateBatch& command);
    void handle(const GetBatch& command);
    void handle(const ChangeState& command);
    void handle(const FinishBatch& command);
    void handle(const ListFinished& command);
    void handle(const GetShrinkage& command);

    std::string name_;
    BatchRepository& repository_;
    ConsoleLog& log_;
    BlockingQueue<Command> queue_;
    std::thread thread_;  // must stay last: it starts using the members above
};

}  // namespace prisma