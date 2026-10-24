#include "prisma/batch_repository.hpp"

#include <functional>
#include <iostream>
#include <thread>

// Temporary smoke test: two threads work on the same repository at once.
// It will be replaced by the console command loop.
namespace {

void roastBatches(prisma::BatchRepository& repository, int count) {
    using prisma::BatchState;

    for (int i = 0; i < count; ++i) {
        const auto id = repository.create(100.0);
        if (!id) {
            continue;
        }
        repository.changeState(*id, BatchState::Roasting);
        repository.changeState(*id, BatchState::Cooling);
        repository.finish(*id, 85.0);
    }
}

}  // namespace

int main() {
    prisma::BatchRepository repository;
    constexpr int batchesPerThread = 1000;

    std::thread first(roastBatches, std::ref(repository), batchesPerThread);
    std::thread second(roastBatches, std::ref(repository), batchesPerThread);
    first.join();
    second.join();

    std::cout << "Batches created:  " << repository.size() << '\n'
              << "Batches finished: " << repository.finished().size() << '\n';

    if (const auto batch = repository.findById(1)) {
        std::cout << "Batch 1 shrinkage: " << batch->shrinkagePercent().value_or(0.0) << " %\n";
    }

    std::cout << "Finish batch 1 again: " << prisma::toString(repository.finish(1, 80.0)) << '\n'
              << "Create with -5 kg:    " << (repository.create(-5.0) ? "created" : "rejected") << '\n'
              << "Find batch 9999:      " << (repository.findById(9999) ? "found" : "not found") << '\n';

    return 0;
}