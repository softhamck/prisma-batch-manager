#include "prisma/batch.hpp"

#include <gtest/gtest.h>

using prisma::Batch;
using prisma::BatchState;

TEST(BatchStateTest, ParseIsCaseInsensitive) {
    EXPECT_EQ(prisma::parseState("roasting"), BatchState::Roasting);
    EXPECT_EQ(prisma::parseState("COOLING"), BatchState::Cooling);
    EXPECT_EQ(prisma::parseState("Cancelled"), BatchState::Cancelled);
}

TEST(BatchStateTest, ParseRejectsUnknownText) {
    EXPECT_FALSE(prisma::parseState("burning").has_value());
    EXPECT_FALSE(prisma::parseState("").has_value());
}

TEST(BatchStateTest, AllowsTheRoastingSequence) {
    EXPECT_TRUE(prisma::canTransition(BatchState::Created, BatchState::Roasting));
    EXPECT_TRUE(prisma::canTransition(BatchState::Roasting, BatchState::Cooling));
    EXPECT_TRUE(prisma::canTransition(BatchState::Cooling, BatchState::Finished));
}

TEST(BatchStateTest, RejectsSkippingOrGoingBack) {
    EXPECT_FALSE(prisma::canTransition(BatchState::Created, BatchState::Cooling));
    EXPECT_FALSE(prisma::canTransition(BatchState::Created, BatchState::Finished));
    EXPECT_FALSE(prisma::canTransition(BatchState::Cooling, BatchState::Roasting));
}

TEST(BatchStateTest, ActiveBatchesCanBeCancelled) {
    EXPECT_TRUE(prisma::canTransition(BatchState::Created, BatchState::Cancelled));
    EXPECT_TRUE(prisma::canTransition(BatchState::Roasting, BatchState::Cancelled));
    EXPECT_TRUE(prisma::canTransition(BatchState::Cooling, BatchState::Cancelled));
}

TEST(BatchStateTest, TerminalStatesNeverChange) {
    for (const auto to : {BatchState::Created, BatchState::Roasting, BatchState::Cooling,
                          BatchState::Finished, BatchState::Cancelled}) {
        EXPECT_FALSE(prisma::canTransition(BatchState::Finished, to));
        EXPECT_FALSE(prisma::canTransition(BatchState::Cancelled, to));
    }
}

TEST(BatchTest, ShrinkageNeedsFinalWeight) {
    const Batch batch{1, 120.0, std::nullopt, BatchState::Cooling};
    EXPECT_FALSE(batch.shrinkagePercent().has_value());
}

TEST(BatchTest, ShrinkageIsLostWeightOverInitialWeight) {
    const Batch batch{1, 120.0, 102.0, BatchState::Finished};
    ASSERT_TRUE(batch.shrinkagePercent().has_value());
    EXPECT_DOUBLE_EQ(*batch.shrinkagePercent(), 15.0);
}