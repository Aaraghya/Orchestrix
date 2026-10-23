#include "AllocationStrategy.h"

#include <limits>
#include <string>

// ─── FirstFit ────────────────────────────────────────────────────────────────

std::string FirstFit::name() const { return "FirstFit"; }

Server* FirstFit::select(
        const std::vector<std::unique_ptr<Server>>& servers,
        const ResourceRequirements& req) const
{
    // Iterate in configured (insertion) order; return the first match.
    for (const auto& s : servers) {
        if (s->canAccommodate(req)) {
            return s.get();
        }
    }
    return nullptr;
}

// ─── BestFit ─────────────────────────────────────────────────────────────────

std::string BestFit::name() const { return "BestFit"; }

double BestFit::score(const Server& server,
                      const ResourceRequirements& req) noexcept
{
    // Score = sum of normalised residual capacities across required dimensions.
    //
    //   residual_d = (avail_d - req_d) / total_d
    //             = (avail_after_placement_d) / total_d
    //
    // Only dimensions where req > 0 are included (spec §6.2).
    // Division by zero cannot occur in the scoring path: canAccommodate()
    // requires req_d <= avail_d, so if req_d > 0 then avail_d > 0, which
    // in turn requires total_d > 0 (since allocated_d >= 0).

    const auto& cap   = server.getCapacity();
    const auto  avail = server.getAvailable();

    double s = 0.0;

    if (req.cpu > 0.0) {
        s += (avail.cpu - req.cpu) / cap.cpu;
    }
    if (req.ram > 0.0) {
        s += (avail.ram - req.ram) / cap.ram;
    }
    if (req.storage > 0.0) {
        s += (avail.storage - req.storage) / cap.storage;
    }
    if (req.gpu > 0.0) {
        s += (avail.gpu - req.gpu) / cap.gpu;
    }

    return s;
}

Server* BestFit::select(
        const std::vector<std::unique_ptr<Server>>& servers,
        const ResourceRequirements& req) const
{
    Server* best      = nullptr;
    double  bestScore = std::numeric_limits<double>::infinity();

    for (const auto& s : servers) {
        if (!s->canAccommodate(req)) {
            continue;
        }

        const double sc = score(*s, req);

        // Prefer lower score; break ties with lexicographically smaller ID.
        if (sc < bestScore ||
            (sc == bestScore && best != nullptr &&
             s->getId() < best->getId())) {
            best      = s.get();
            bestScore = sc;
        }
    }

    return best;
}

// ─── LoadBalanced ─────────────────────────────────────────────────────────────

std::string LoadBalanced::name() const { return "LoadBalanced"; }

double LoadBalanced::score(const Server& server,
                           const ResourceRequirements& req) noexcept
{
    // Score = average projected utilisation across dimensions with total > 0.
    //
    //   util_d = (allocated_d + req_d) / total_d
    //
    // All four dimensions are considered if their total > 0
    // (simulation-rules.md §6.3: "dimensions with positive total capacity").
    // If a dimension has total == 0 it is excluded from both numerator and
    // denominator, so a server with no GPU capacity is not penalised for it.

    const auto& cap   = server.getCapacity();
    const auto  alloc = server.getAllocated();

    double sum   = 0.0;
    int    count = 0;

    auto addDim = [&](double allocated, double required, double total) {
        if (total > 0.0) {
            sum += (allocated + required) / total;
            ++count;
        }
    };

    addDim(alloc.cpu,     req.cpu,     cap.cpu);
    addDim(alloc.ram,     req.ram,     cap.ram);
    addDim(alloc.storage, req.storage, cap.storage);
    addDim(alloc.gpu,     req.gpu,     cap.gpu);

    // If no dimension has positive total capacity the server is degenerate.
    // Return a very high score so it is never selected.
    if (count == 0) {
        return std::numeric_limits<double>::infinity();
    }

    return sum / static_cast<double>(count);
}

Server* LoadBalanced::select(
        const std::vector<std::unique_ptr<Server>>& servers,
        const ResourceRequirements& req) const
{
    Server* best      = nullptr;
    double  bestScore = std::numeric_limits<double>::infinity();

    for (const auto& s : servers) {
        if (!s->canAccommodate(req)) {
            continue;
        }

        const double sc = score(*s, req);

        // Prefer lower score; break ties with lexicographically smaller ID.
        if (sc < bestScore ||
            (sc == bestScore && best != nullptr &&
             s->getId() < best->getId())) {
            best      = s.get();
            bestScore = sc;
        }
    }

    return best;
}
