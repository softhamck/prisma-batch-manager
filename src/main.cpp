#include "prisma/batch.hpp"

#include <iostream>

// Temporary smoke test for the batch model.
// It will be replaced by the console command loop.
int main()
{
    using prisma::BatchState;

    prisma::Batch batch{1, 120.0, std::nullopt, BatchState::Created};
    std::cout << "Batch " << batch.id << " is " << prisma::toString(batch.state) << '\n';

    std::cout << std::boolalpha
              << "Created -> Roasting allowed: "
              << prisma::canTransition(BatchState::Created, BatchState::Roasting) << '\n'
              << "Finished -> Roasting allowed: "
              << prisma::canTransition(BatchState::Finished, BatchState::Roasting) << '\n';

    batch.state = BatchState::Finished;
    batch.finalWeightKg = 102.0;
    std::cout << "Shrinkage: " << batch.shrinkagePercent().value_or(0.0) << " %\n";

    return 0;
}