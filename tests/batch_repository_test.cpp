#include "prisma/batch_repository.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <limits>
#include <set>
#include <thread>
#include <vector>

using prisma::BatchId;
using prisma::BatchRepository;
using prisma::BatchState;
using prisma::UpdateResult;

namespace {

constexpr int threadCount = 8;
constexpr int batchesPerThread = 1000;
constexpr int writerCount = 4;
constexpr int batchesPerWriter = 500;

BatchId createCooledBatch(BatchRepository& repository, double initialKg) {
    const BatchId id = *repository.create(initialKg);
    repository.changeState(id, BatchState::Roasting);
    repository.changeState(id, BatchState::Cooling);
    return id;
}

}  // namespace

TEST(BatchRepositoryTest, CreateAssignsConsecutiveIds) {
    BatchRepository repository;
    EXPECT_EQ(repository.create(100.0), BatchId{1});
    EXPECT_EQ(repository.create(80.0), BatchId{2});
    EXPECT_EQ(repository.size(), 2u);
}

TEST(BatchRepositoryTest, CreateRejectsInvalidWeights) {
    BatchRepository repository;
    EXPECT_FALSE(repository.create(0.0).has_value());
    EXPECT_FALSE(repository.create(-5.0).has_value());
    EXPECT_FALSE(repository.create(std::numeric_limits<double>::quiet_NaN()).has_value());
    EXPECT_FALSE(repository.create(std::numeric_limits<double>::infinity()).has_value());
    EXPECT_EQ(repository.size(), 0u);
}

TEST(BatchRepositoryTest, FindByIdReturnsACopyOfTheBatch) {
    BatchRepository repository;
    const BatchId id = *repository.create(120.0);

    const auto batch = repository.findById(id);
    ASSERT_TRUE(batch.has_value());
    EXPECT_EQ(batch->id, id);
    EXPECT_DOUBLE_EQ(batch->initialWeightKg, 120.0);
    EXPECT_EQ(batch->state, BatchState::Created);
    EXPECT_FALSE(repository.findById(999).has_value());
}

TEST(BatchRepositoryTest, ChangeStateFollowsTheStateMachine) {
    BatchRepository repository;
    const BatchId id = *repository.create(120.0);

    EXPECT_EQ(repository.changeState(id, BatchState::Cooling), UpdateResult::InvalidTransition);
    EXPECT_EQ(repository.changeState(id, BatchState::Roasting), UpdateResult::Ok);
    EXPECT_EQ(repository.findById(id)->state, BatchState::Roasting);
    EXPECT_EQ(repository.changeState(999, BatchState::Roasting), UpdateResult::NotFound);
}

TEST(BatchRepositoryTest, FinishedStateRequiresFinalWeight) {
    BatchRepository repository;
    const BatchId id = createCooledBatch(repository, 120.0);

    EXPECT_EQ(repository.changeState(id, BatchState::Finished), UpdateResult::FinalWeightRequired);
    EXPECT_EQ(repository.findById(id)->state, BatchState::Cooling);
}

TEST(BatchRepositoryTest, FinishValidatesFinalWeight) {
    BatchRepository repository;
    const BatchId id = createCooledBatch(repository, 120.0);

    EXPECT_EQ(repository.finish(id, 130.0), UpdateResult::InvalidWeight);
    EXPECT_EQ(repository.finish(id, 0.0), UpdateResult::InvalidWeight);
    EXPECT_EQ(repository.finish(id, 102.0), UpdateResult::Ok);

    const auto batch = repository.findById(id);
    EXPECT_EQ(batch->state, BatchState::Finished);
    EXPECT_DOUBLE_EQ(*batch->shrinkagePercent(), 15.0);
}

TEST(BatchRepositoryTest, FinishedListsOnlyFinishedBatchesInIdOrder) {
    BatchRepository repository;
    const BatchId first = createCooledBatch(repository, 100.0);
    repository.create(90.0);
    const BatchId third = createCooledBatch(repository, 80.0);
    repository.finish(third, 70.0);
    repository.finish(first, 85.0);

    const auto finished = repository.finished();
    ASSERT_EQ(finished.size(), 2u);
    EXPECT_EQ(finished[0].id, first);
    EXPECT_EQ(finished[1].id, third);
}

// --- Concurrency -----------------------------------------------------------

TEST(BatchRepositoryConcurrencyTest, ConcurrentCreatesNeverRepeatIds) {
    BatchRepository repository;

    std::vector<std::vector<BatchId>> idsPerThread(threadCount);
    std::vector<std::thread> threads;
    for (int t = 0; t < threadCount; ++t) {
        threads.emplace_back([&repository, &ids = idsPerThread[t]] {
            for (int i = 0; i < batchesPerThread; ++i) {
                ids.push_back(*repository.create(100.0));
            }
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }

    std::set<BatchId> uniqueIds;
    for (const auto& ids : idsPerThread) {
        uniqueIds.insert(ids.begin(), ids.end());
    }
    EXPECT_EQ(uniqueIds.size(), static_cast<std::size_t>(threadCount * batchesPerThread));
    EXPECT_EQ(repository.size(), uniqueIds.size());
}

TEST(BatchRepositoryConcurrencyTest, OnlyOneThreadCanFinishABatch) {
    BatchRepository repository;
    const BatchId id = createCooledBatch(repository, 100.0);

    std::atomic<int> successes{0};
    std::vector<std::thread> threads;
    for (int t = 0; t < threadCount; ++t) {
        threads.emplace_back([&] {
            if (repository.finish(id, 85.0) == UpdateResult::Ok) {
                ++successes;
            }
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(successes.load(), 1);
}

TEST(BatchRepositoryConcurrencyTest, ReadersAndWritersKeepDataConsistent) {
    BatchRepository repository;
    std::atomic<bool> writersDone{false};

    std::vector<std::thread> writers;
    for (int w = 0; w < writerCount; ++w) {
        writers.emplace_back([&repository] {
            for (int i = 0; i < batchesPerWriter; ++i) {
                const BatchId id = createCooledBatch(repository, 100.0);
                repository.finish(id, 85.0);
            }
        });
    }

    // Every batch a reader sees as Finished must already carry its final weight.
    std::thread reader([&] {
        while (!writersDone) {
            for (const auto& batch : repository.finished()) {
                ASSERT_TRUE(batch.finalWeightKg.has_value());
            }
        }
    });

    for (auto& writer : writers) {
        writer.join();
    }
    writersDone = true;
    reader.join();

    EXPECT_EQ(repository.finished().size(),
              static_cast<std::size_t>(writerCount * batchesPerWriter));
}
