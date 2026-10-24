#pragma once

#include "prisma/batch.hpp"

#include <cstddef>
#include <map>
#include <optional>
#include <shared_mutex>
#include <string_view>
#include <vector>

namespace prisma {

enum class UpdateResult {
    Ok,
    NotFound,
    InvalidTransition,
    InvalidWeight,
    FinalWeightRequired
};

std::string_view toString(UpdateResult result);

// Single owner of the batch data shared by every thread.
// All access goes through these methods, so callers never touch the map
// or the mutex directly.
class BatchRepository {
public:
    std::optional<BatchId> create(double initialWeightKg);
    std::optional<Batch> findById(BatchId id) const;

    UpdateResult changeState(BatchId id, BatchState newState);
    UpdateResult finish(BatchId id, double finalWeightKg);

    std::vector<Batch> finished() const;
    std::size_t size() const;

private:
    mutable std::shared_mutex mutex_;
    std::map<BatchId, Batch> batches_;
    BatchId nextId_{1};
};

}  // namespace prisma
