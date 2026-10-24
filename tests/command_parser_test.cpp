#include "prisma/command_parser.hpp"

#include <gtest/gtest.h>

#include <variant>

using namespace prisma;

namespace {

const RoutedCommand* routed(const ConsoleInput& input) {
    return std::get_if<RoutedCommand>(&input);
}

template <typename T>
const T* commandOf(const ConsoleInput& input) {
    const auto* result = routed(input);
    return result ? std::get_if<T>(&result->command) : nullptr;
}

bool isError(const ConsoleInput& input) {
    return std::holds_alternative<ParseError>(input);
}

}  // namespace

TEST(CommandParserTest, RoutesCommandToTheRequestedWorker) {
    const auto toFirst = parseLine("t1 create 120.5");
    const auto toSecond = parseLine("t2 finished");

    ASSERT_NE(routed(toFirst), nullptr);
    EXPECT_EQ(routed(toFirst)->worker, 0u);
    ASSERT_NE(routed(toSecond), nullptr);
    EXPECT_EQ(routed(toSecond)->worker, 1u);
}

TEST(CommandParserTest, ParsesEveryCommand) {
    const auto create = parseLine("t1 create 120.5");
    ASSERT_NE(commandOf<CreateBatch>(create), nullptr);
    EXPECT_DOUBLE_EQ(commandOf<CreateBatch>(create)->initialWeightKg, 120.5);

    const auto state = parseLine("t1 state 3 roasting");
    ASSERT_NE(commandOf<ChangeState>(state), nullptr);
    EXPECT_EQ(commandOf<ChangeState>(state)->id, BatchId{3});
    EXPECT_EQ(commandOf<ChangeState>(state)->newState, BatchState::Roasting);

    const auto finish = parseLine("t1 finish 3 102");
    ASSERT_NE(commandOf<FinishBatch>(finish), nullptr);
    EXPECT_DOUBLE_EQ(commandOf<FinishBatch>(finish)->finalWeightKg, 102.0);

    EXPECT_NE(commandOf<GetBatch>(parseLine("t1 get 3")), nullptr);
    EXPECT_NE(commandOf<GetShrinkage>(parseLine("t1 shrink 3")), nullptr);
    EXPECT_NE(commandOf<ListFinished>(parseLine("t1 finished")), nullptr);
}

TEST(CommandParserTest, IgnoresCaseAndExtraSpaces) {
    const auto input = parseLine("   T2   STATE  3   Cooling  ");
    ASSERT_NE(commandOf<ChangeState>(input), nullptr);
    EXPECT_EQ(routed(input)->worker, 1u);
    EXPECT_EQ(commandOf<ChangeState>(input)->newState, BatchState::Cooling);
}

TEST(CommandParserTest, RecognizesHelpAndExit) {
    EXPECT_TRUE(std::holds_alternative<ShowHelp>(parseLine("help")));
    EXPECT_TRUE(std::holds_alternative<Quit>(parseLine("EXIT")));
}

TEST(CommandParserTest, RejectsUnknownTargetsAndCommands) {
    EXPECT_TRUE(isError(parseLine("t3 get 1")));
    EXPECT_TRUE(isError(parseLine("t1 bake 1")));
    EXPECT_TRUE(isError(parseLine("t1")));
    EXPECT_TRUE(isError(parseLine("")));
}

TEST(CommandParserTest, RejectsMalformedNumbers) {
    EXPECT_TRUE(isError(parseLine("t1 create abc")));
    EXPECT_TRUE(isError(parseLine("t1 create 12abc")));
    EXPECT_TRUE(isError(parseLine("t1 get 1.5")));
    EXPECT_TRUE(isError(parseLine("t1 get -1")));
    EXPECT_TRUE(isError(parseLine("t1 state 1 burning")));
}

TEST(CommandParserTest, RejectsWrongArgumentCount) {
    EXPECT_TRUE(isError(parseLine("t1 create")));
    EXPECT_TRUE(isError(parseLine("t1 create 100 200")));
    EXPECT_TRUE(isError(parseLine("t1 finish 1")));
    EXPECT_TRUE(isError(parseLine("t1 finished now")));
}

// Business rules live in the repository, so a well formed negative weight
// must reach it instead of being rejected here.
TEST(CommandParserTest, LeavesBusinessRulesToTheRepository) {
    const auto input = parseLine("t1 create -5");
    ASSERT_NE(commandOf<CreateBatch>(input), nullptr);
    EXPECT_DOUBLE_EQ(commandOf<CreateBatch>(input)->initialWeightKg, -5.0);
}
