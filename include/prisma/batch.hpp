#pragma once //Evita que el archivo se incluya dos veces

#include <cstdint>
#include <optional>
#include <string_view>

namespace prisma {

enum class BatchState {
    Created,
    Roasting,
    Cooling,
    Finished,
    Cancelled
};

std::string_view toString(BatchState state);
std::optional<BatchState> parseState(std::string_view text);

bool isTerminal(BatchState state);
bool canTransition(BatchState from, BatchState to);

using BatchId = std::uint64_t;

struct Batch {
    BatchId id{};
    double initialWeightKg{};
    std::optional<double> finalWeightKg;  // known only once the batch is finished
    BatchState state{BatchState::Created};

    std::optional<double> shrinkagePercent() const;
};

}  // namespace prisma