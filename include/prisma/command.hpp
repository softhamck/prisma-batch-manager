#pragma once

#include "prisma/batch.hpp"

#include <variant>

namespace prisma {

struct CreateBatch {
    double initialWeightKg{};
};

struct GetBatch {
    BatchId id{};
};

struct ChangeState {
    BatchId id{};
    BatchState newState{};
};

struct FinishBatch {
    BatchId id{};
    double finalWeightKg{};
};

struct ListFinished {};

struct GetShrinkage {
    BatchId id{};
};

using Command =
    std::variant<CreateBatch, GetBatch, ChangeState, FinishBatch, ListFinished, GetShrinkage>;

}  // namespace prisma
