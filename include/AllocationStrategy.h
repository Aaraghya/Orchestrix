#pragma once

#include "ResourceRequirements.h"
#include "Server.h"

#include <memory>
#include <string>
#include <vector>

/// Abstract interface for server-selection strategies.
///
/// Each concrete strategy implements a single policy for choosing which server
/// should receive a given job.  The interface deliberately does NOT reserve
/// resources: that responsibility belongs to the caller (architecture.md §3.5–§3.7,
/// simulation-rules.md §6 and §107).
///
/// The strategy receives:
///  - A read-only view of the server list (via const reference to the vector
///    owned by DataCenter).
///  - The resource requirements of the job being placed.
///
/// It returns a non-owning pointer to the selected server, or nullptr when no
/// compatible, feasible server exists.
///
/// Rationale for returning a raw pointer:
///  - The DataCenter owns the servers; the strategy just identifies one.
///  - Returning a raw (non-owning) pointer is idiomatic for "look-up" results
///    when the caller clearly outlives the pointer.
///  - An optional<Server&> is not allowed in C++17 standard; optional<Server*>
///    adds indirection without clarity; nullptr is the clearest sentinel.
class AllocationStrategy {
public:
    virtual ~AllocationStrategy() = default;

    /// Selects a server for a job with the given resource requirements.
    ///
    /// \param servers  The complete server pool (from DataCenter::getServers()).
    /// \param req      Resources the job requires.
    ///
    /// \return A non-owning pointer to the chosen server, or nullptr if no
    ///         compatible, feasible server exists.
    ///
    /// Guarantees:
    ///  - No server state is modified.
    ///  - The returned pointer is into the \p servers vector; the caller must
    ///    not use it after modifying that vector.
    [[nodiscard]] virtual Server* select(
        const std::vector<std::unique_ptr<Server>>& servers,
        const ResourceRequirements& req) const = 0;

    /// Short human-readable label (e.g. "FirstFit") for logging and comparison.
    [[nodiscard]] virtual std::string name() const = 0;
};

// ─── Concrete strategies ──────────────────────────────────────────────────────

/// First Fit: return the first server (in configured order) that can
/// accommodate the job.  (simulation-rules.md §6.1)
class FirstFit : public AllocationStrategy {
public:
    [[nodiscard]] Server* select(
        const std::vector<std::unique_ptr<Server>>& servers,
        const ResourceRequirements& req) const override;

    [[nodiscard]] std::string name() const override;
};

/// Best Fit: among feasible servers, select the one that minimises the sum
/// of normalised residual capacities across the dimensions required by the job.
///
/// Normalisation and scoring
/// ─────────────────────────
/// For each resource dimension d where req.d > 0:
///
///   residual_d = (available_d - req.d) / total_d
///
/// score = Σ residual_d  (summed over dimensions where req.d > 0)
///
/// The server with the smallest score is the tightest fit and is preferred.
/// If a dimension has req == 0 it is excluded (simulation-rules.md §6.2:
/// "Exclude resource dimensions not required by the job").
///
/// Edge case — total_d == 0 while req.d > 0:
///   This means the job requires a resource the server has no capacity for.
///   canAccommodate() would already return false (0 available < req > 0), so
///   the server is not a candidate.  No special division-by-zero handling is
///   needed in the scoring path.
///
/// Tie-breaking: lexicographic comparison of server ID (smaller ID wins).
/// (simulation-rules.md §6.2)
class BestFit : public AllocationStrategy {
public:
    [[nodiscard]] Server* select(
        const std::vector<std::unique_ptr<Server>>& servers,
        const ResourceRequirements& req) const override;

    [[nodiscard]] std::string name() const override;

private:
    /// Computes the Best Fit score for one feasible server.
    [[nodiscard]] static double score(const Server& server,
                                      const ResourceRequirements& req) noexcept;
};

/// Load Balanced: among feasible servers, select the one that will have the
/// lowest average projected utilisation after the job is placed.
///
/// Utilisation and scoring
/// ───────────────────────
/// Projected utilisation for dimension d (only where total_d > 0):
///
///   util_d = (allocated_d + req.d) / total_d
///
/// score = (Σ util_d) / (number of dimensions with total_d > 0)
///
/// The server with the smallest score is preferred (least loaded after placement).
///
/// Tie-breaking: lexicographic comparison of server ID (smaller ID wins).
/// (simulation-rules.md §6.3)
class LoadBalanced : public AllocationStrategy {
public:
    [[nodiscard]] Server* select(
        const std::vector<std::unique_ptr<Server>>& servers,
        const ResourceRequirements& req) const override;

    [[nodiscard]] std::string name() const override;

private:
    /// Computes the Load Balanced score for one feasible server.
    [[nodiscard]] static double score(const Server& server,
                                      const ResourceRequirements& req) noexcept;
};
