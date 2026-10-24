#include "prisma/worker.hpp"

#include <iostream>
#include <string>

// Temporary smoke test: two workers receive commands and run them at the
// same time on the shared repository. Replaced by the console loop next.
int main() {
    using namespace prisma;

    BatchRepository repository;
    ConsoleLog log(std::cout);

    {
        Worker first("T1", repository, log);
        Worker second("T2", repository, log);

        first.submit(CreateBatch{120.0});
        first.submit(ChangeState{1, BatchState::Roasting});
        first.submit(ChangeState{1, BatchState::Cooling});
        first.submit(FinishBatch{1, 102.0});
        first.submit(ListFinished{});

        second.submit(CreateBatch{95.5});
        second.submit(GetBatch{1});
        second.submit(ChangeState{2, BatchState::Cooling});
        second.submit(GetShrinkage{1});
    }  // both workers drain their queues and join here

    log.write("main", "Total batches: " + std::to_string(repository.size()) +
                          ", finished: " + std::to_string(repository.finished().size()));
    return 0;
}
