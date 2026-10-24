#include "prisma/batch.hpp"

#include <algorithm>
#include <cctype>
#include <string>

namespace prisma
{
    std::string_view toString(BatchState state)
    {
        switch (state)
        {
        case BatchState::Created:
            return "Created";
        case BatchState::Roasting:
            return "Roasting";
        case BatchState::Cooling:
            return "Cooling";
        case BatchState::Finished:
            return "Finished";
        case BatchState::Cancelled:
            return "Cancelled";
        }
        return "Unknown";
    }

    std::optional<BatchState> parseState(std::string_view text)
    {
        std::string value(text);
        std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c)
        { return static_cast<char>(std::tolower(c)); });

        if (value == "created")
            return BatchState::Created;
        if (value == "roasting")
            return BatchState::Roasting;
        if (value == "cooling")
            return BatchState::Cooling;
        if (value == "finished")
            return BatchState::Finished;
        if (value == "cancelled")
            return BatchState::Cancelled;
        return std::nullopt;
    }

    bool isTerminal(BatchState state)
    {
        return state == BatchState::Finished || state == BatchState::Cancelled;
    }

    // Created -> Roasting -> Cooling -> Finished.
    // Any non-terminal batch can be cancelled; terminal batches never change again.
    bool canTransition(BatchState from, BatchState to)
    {
        if (isTerminal(from))
        {
            return false;
        }
        if (to == BatchState::Cancelled)
        {
            return true;
        }

        switch (from)
        {
        case BatchState::Created:
            return to == BatchState::Roasting;
        case BatchState::Roasting:
            return to == BatchState::Cooling;
        case BatchState::Cooling:
            return to == BatchState::Finished;
        default:
            return false;
        }
    }

    std::optional<double> Batch::shrinkagePercent() const
    {
        if (!finalWeightKg || initialWeightKg <= 0.0)
        {
            return std::nullopt;
        }
        return (initialWeightKg - *finalWeightKg) / initialWeightKg * 100.0;
    }

} // namespace prisma
