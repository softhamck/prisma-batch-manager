#include "prisma/worker.hpp"

#include <exception>
#include <iomanip>
#include <sstream>
#include <utility>

namespace prisma {

namespace {

std::string formatNumber(double value) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(2) << value;
    return out.str();
}

std::string label(BatchId id) {
    return "Batch " + std::to_string(id);
}

std::string describe(const Batch& batch) {
    std::string text = label(batch.id) + " | " + std::string(toString(batch.state)) +
                       " | initial " + formatNumber(batch.initialWeightKg) + " kg";
    if (batch.finalWeightKg) {
        text += " | final " + formatNumber(*batch.finalWeightKg) + " kg";
    }
    if (const auto shrinkage = batch.shrinkagePercent()) {
        text += " | shrinkage " + formatNumber(*shrinkage) + " %";
    }
    return text;
}

}  // namespace

Worker::Worker(std::string name, BatchRepository& repository, ConsoleLog& log)
    : name_(std::move(name)),
      repository_(repository),
      log_(log),
      thread_(&Worker::run, this) {}

Worker::~Worker() {
    stop();
}

bool Worker::submit(Command command) {
    return queue_.push(std::move(command));
}

void Worker::stop() {
    queue_.close();
    if (thread_.joinable()) {
        thread_.join();
    }
}

void Worker::run() {
    while (auto command = queue_.pop()) {
        try {
            std::visit([this](const auto& cmd) { handle(cmd); }, *command);
        } catch (const std::exception& error) {
            log_.write(name_, std::string("Unexpected error: ") + error.what());
        }
    }
}

void Worker::handle(const CreateBatch& command) {
    const auto id = repository_.create(command.initialWeightKg);
    if (!id) {
        log_.write(name_, "Invalid initial weight");
        return;
    }
    log_.write(name_,
               "Created " + label(*id) + " with " + formatNumber(command.initialWeightKg) + " kg");
}

void Worker::handle(const GetBatch& command) {
    const auto batch = repository_.findById(command.id);
    log_.write(name_, batch ? describe(*batch) : label(command.id) + " not found");
}

void Worker::handle(const ChangeState& command) {
    const auto result = repository_.changeState(command.id, command.newState);
    if (result != UpdateResult::Ok) {
        log_.write(name_, label(command.id) + ": " + std::string(toString(result)));
        return;
    }
    log_.write(name_, label(command.id) + " -> " + std::string(toString(command.newState)));
}

void Worker::handle(const FinishBatch& command) {
    const auto result = repository_.finish(command.id, command.finalWeightKg);
    if (result != UpdateResult::Ok) {
        log_.write(name_, label(command.id) + ": " + std::string(toString(result)));
        return;
    }
    log_.write(name_, label(command.id) + " -> Finished with " +
                          formatNumber(command.finalWeightKg) + " kg");
}

void Worker::handle(const ListFinished&) {
    const auto batches = repository_.finished();
    if (batches.empty()) {
        log_.write(name_, "No finished batches");
        return;
    }

    std::string message = std::to_string(batches.size()) + " finished batch(es):";
    for (const auto& batch : batches) {
        message += "\n    " + describe(batch);
    }
    log_.write(name_, message);
}

void Worker::handle(const GetShrinkage& command) {
    const auto batch = repository_.findById(command.id);
    if (!batch) {
        log_.write(name_, label(command.id) + " not found");
        return;
    }

    const auto shrinkage = batch->shrinkagePercent();
    if (!shrinkage) {
        log_.write(name_, label(command.id) + " is not finished yet (" +
                              std::string(toString(batch->state)) + ")");
        return;
    }
    log_.write(name_, label(command.id) + " shrinkage: " + formatNumber(*shrinkage) + " %");
}

}  // namespace prisma
