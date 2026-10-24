#include "prisma/batch_repository.hpp"

#include <cmath>
#include <mutex>

namespace prisma
{

    namespace
    {

    bool isValidInitialWeight(double kg)
    {
        return std::isfinite(kg) && kg > 0.0;
    }

    // Roasting only removes mass (mostly water), so the final weight
    // can never exceed the initial one.
    bool isValidFinalWeight(double initialKg, double finalKg)
    {
        return std::isfinite(finalKg) && finalKg > 0.0 && finalKg <= initialKg;
    }

    } // namespace

    std::string_view toString(UpdateResult result)
    {
        switch (result)
        {
        case UpdateResult::Ok:
            return "Ok";
        case UpdateResult::NotFound:
            return "Batch not found";
        case UpdateResult::InvalidTransition:
            return "Invalid state transition";
        case UpdateResult::InvalidWeight:
            return "Invalid weight";
        case UpdateResult::FinalWeightRequired:
            return "Final weight required, use finish";
        }
        return "Unknown";
    }

    std::optional<BatchId> BatchRepository::create(double initialWeightKg)
    {
        if (!isValidInitialWeight(initialWeightKg))
        {
            return std::nullopt;
        }

        std::unique_lock lock(mutex_);
        const BatchId id = nextId_++;
        batches_.emplace(id, Batch{id, initialWeightKg, std::nullopt, BatchState::Created});
        return id;
    }

    std::optional<Batch> BatchRepository::findById(BatchId id) const
    {
        std::shared_lock lock(mutex_);
        const auto it = batches_.find(id);
        if (it == batches_.end())
        {
            return std::nullopt;
        }
        return it->second;
    }

    UpdateResult BatchRepository::changeState(BatchId id, BatchState newState)
    {
        if (newState == BatchState::Finished)
        {
            return UpdateResult::FinalWeightRequired;
        }

        std::unique_lock lock(mutex_);
        const auto it = batches_.find(id);
        if (it == batches_.end())
        {
            return UpdateResult::NotFound;
        }

        Batch &batch = it->second;
        if (!canTransition(batch.state, newState))
        {
            return UpdateResult::InvalidTransition;
        }
        batch.state = newState;
        return UpdateResult::Ok;
    }

    UpdateResult BatchRepository::finish(BatchId id, double finalWeightKg)
    {
        std::unique_lock lock(mutex_);
        const auto it = batches_.find(id);
        if (it == batches_.end())
        {
            return UpdateResult::NotFound;
        }

        Batch &batch = it->second;
        if (!canTransition(batch.state, BatchState::Finished))
        {
            return UpdateResult::InvalidTransition;
        }
        if (!isValidFinalWeight(batch.initialWeightKg, finalWeightKg))
        {
            return UpdateResult::InvalidWeight;
        }
        batch.finalWeightKg = finalWeightKg;
        batch.state = BatchState::Finished;
        return UpdateResult::Ok;
    }

    std::vector<Batch> BatchRepository::finished() const
    {
        std::shared_lock lock(mutex_);
        std::vector<Batch> result;
        for (const auto &[id, batch] : batches_)
        {
            if (batch.state == BatchState::Finished)
            {
                result.push_back(batch);
            }
        }
        return result;
    }

    std::size_t BatchRepository::size() const
    {
        std::shared_lock lock(mutex_);
        return batches_.size();
    }

} // namespace prisma
