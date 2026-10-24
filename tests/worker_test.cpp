#include "prisma/worker.hpp"

#include <gtest/gtest.h>

#include <sstream>
#include <string>

using namespace prisma;

TEST(WorkerTest, ExecutesCommandsAndReportsResults) {
    BatchRepository repository;
    std::ostringstream output;
    ConsoleLog log(output);

    {
        Worker worker("T1", repository, log);
        worker.submit(CreateBatch{120.0});
        worker.submit(ChangeState{1, BatchState::Roasting});
        worker.submit(ChangeState{1, BatchState::Cooling});
        worker.submit(FinishBatch{1, 102.0});
        worker.submit(GetShrinkage{1});
    }

    EXPECT_EQ(repository.findById(1)->state, BatchState::Finished);

    const std::string text = output.str();
    EXPECT_NE(text.find("[T1] Created Batch 1 with 120.00 kg"), std::string::npos);
    EXPECT_NE(text.find("[T1] Batch 1 -> Finished with 102.00 kg"), std::string::npos);
    EXPECT_NE(text.find("[T1] Batch 1 shrinkage: 15.00 %"), std::string::npos);
}

TEST(WorkerTest, ReportsRejectedOperations) {
    BatchRepository repository;
    std::ostringstream output;
    ConsoleLog log(output);

    {
        Worker worker("T2", repository, log);
        worker.submit(CreateBatch{-5.0});
        worker.submit(GetBatch{7});
    }

    const std::string text = output.str();
    EXPECT_NE(text.find("[T2] Invalid initial weight"), std::string::npos);
    EXPECT_NE(text.find("[T2] Batch 7 not found"), std::string::npos);
}

TEST(WorkerTest, FinishesPendingCommandsBeforeStopping) {
    BatchRepository repository;
    std::ostringstream output;
    ConsoleLog log(output);

    {
        Worker worker("T1", repository, log);
        for (int i = 0; i < 200; ++i) {
            worker.submit(CreateBatch{100.0});
        }
    }

    EXPECT_EQ(repository.size(), 200u);
}

TEST(WorkerTest, RejectsCommandsAfterStop) {
    BatchRepository repository;
    std::ostringstream output;
    ConsoleLog log(output);

    Worker worker("T1", repository, log);
    worker.stop();

    EXPECT_FALSE(worker.submit(CreateBatch{100.0}));
    EXPECT_EQ(repository.size(), 0u);
}

TEST(WorkerTest, TwoWorkersShareTheSameRepository) {
    BatchRepository repository;
    std::ostringstream output;
    ConsoleLog log(output);

    {
        Worker first("T1", repository, log);
        Worker second("T2", repository, log);
        for (int i = 0; i < 300; ++i) {
            first.submit(CreateBatch{100.0});
            second.submit(CreateBatch{90.0});
        }
    }

    EXPECT_EQ(repository.size(), 600u);
}
