#pragma once

#include "ResourceRequirements.h"
#include <string>
#include <stdexcept>

/// Abstract base class for all server types in the data center.
///
/// Design decisions:
///  - The class owns the resource-accounting logic so that every derived type
///    inherits identical invariant enforcement (architecture.md §3.3).
///  - "Available" resources are never tracked as a separate field: they are
///    computed on demand as (total - allocated).  This eliminates the risk of
///    the two fields drifting apart.
///  - reserveResources() and releaseResources() are atomic: a failed reservation
///    makes no change (simulation-rules.md §4).
///  - canAccommodate() is a pure feasibility check; it does not modify state.
///  - The pure virtual canRunJob() lets derived types add their own
///    compatibility constraints (e.g. GPUServer only accepts jobs that
///    actually request GPUs) without altering the accounting logic.
///  - The server can be marked unavailable (e.g. for a future failure-
///    simulation extension) without removing it from the DataCenter.
class Server {
public:
    /// Constructs a Server with the given total capacity.
    ///
    /// \param id       Non-empty server identifier.
    /// \param capacity Total resource capacity (all fields must be non-negative).
    Server(std::string id, ResourceRequirements capacity);

    virtual ~Server() = default;

    // Non-copyable, movable.
    Server(const Server&)            = delete;
    Server& operator=(const Server&) = delete;
    Server(Server&&)                 = default;
    Server& operator=(Server&&)      = default;

    // ── Accessors ──────────────────────────────────────────────────────────

    [[nodiscard]] const std::string&          getId()           const noexcept;
    [[nodiscard]] const ResourceRequirements& getCapacity()     const noexcept;
    [[nodiscard]] ResourceRequirements        getAllocated()     const noexcept;
    [[nodiscard]] ResourceRequirements        getAvailable()    const noexcept;
    [[nodiscard]] bool                        isAvailable()     const noexcept;

    // ── Resource operations ────────────────────────────────────────────────

    /// Returns true when the server is available AND has enough free resources
    /// AND canRunJob() approves the workload.
    [[nodiscard]] bool canAccommodate(const ResourceRequirements& req) const noexcept;

    /// Reserves \p req from available capacity.
    ///
    /// Throws std::runtime_error when:
    ///   - The server is unavailable.
    ///   - The requested resources exceed available capacity.
    ///   - canRunJob() returns false for this server type.
    ///
    /// Atomicity guarantee: on failure the allocated field is not changed.
    void reserveResources(const ResourceRequirements& req);

    /// Releases \p req back to available capacity.
    ///
    /// Throws std::runtime_error when:
    ///   - Releasing would make allocated resources negative (i.e. there is no
    ///     matching prior reservation for this amount).
    ///
    /// The caller (ResourceManager in a later milestone) is responsible for
    /// preventing double-release of the same logical reservation.
    void releaseResources(const ResourceRequirements& req);

    /// Marks the server as unavailable (e.g. for simulated failure).
    /// Existing allocations are preserved; new reservations will be refused.
    void setAvailable(bool available) noexcept;

    /// Returns a human-readable type label (e.g. "ComputeServer").
    [[nodiscard]] virtual std::string typeName() const = 0;

protected:
    /// Derived classes override this to impose workload compatibility rules.
    /// Return false to prevent this server type from accepting a job.
    [[nodiscard]] virtual bool canRunJob(const ResourceRequirements& req) const noexcept = 0;

private:
    std::string          m_id;
    ResourceRequirements m_capacity;   ///< Total capacity — never changes after construction.
    ResourceRequirements m_allocated;  ///< Resources currently in use.
    bool                 m_available;  ///< Whether the server accepts new allocations.
};

// ─── Concrete server types ────────────────────────────────────────────────────

/// A general-purpose compute server.
///
/// Accepts any job whose requirements fit within its capacity. This is the
/// default workhorse that can run CPU-bound, RAM-bound or mixed workloads.
/// GPU fields in capacity may be zero, in which case GPU-requesting jobs will
/// not fit (the base canAccommodate check handles this automatically).
class ComputeServer : public Server {
public:
    using Server::Server;   // Inherit constructor.

    [[nodiscard]] std::string typeName() const override;

protected:
    /// ComputeServer accepts all workloads whose resources fit.
    [[nodiscard]] bool canRunJob(const ResourceRequirements& req) const noexcept override;
};

/// A GPU-accelerated server.
///
/// Meaningful difference: GPUServer only accepts jobs that request at least
/// one GPU device. A job with gpu == 0 will be rejected even if the server
/// happens to have spare GPU capacity, because placing a CPU-only job on a
/// scarce GPU server wastes a specialised resource.
///
/// This implements the architecture.md §3.3 requirement that "derived server
/// types may add their own compatibility constraints."
class GPUServer : public Server {
public:
    using Server::Server;

    [[nodiscard]] std::string typeName() const override;

protected:
    /// Accepts only jobs that require at least one GPU device.
    [[nodiscard]] bool canRunJob(const ResourceRequirements& req) const noexcept override;
};

/// A storage-optimised server.
///
/// Meaningful difference: StorageServer requires that a job requests at least
/// some storage. CPU-only jobs are directed elsewhere so that high-storage
/// workloads always have access to the specialised server pool.
///
/// Additionally, a StorageServer's CPU and RAM capacities may legitimately be
/// lower than a ComputeServer's; this is reflected by simply giving it a
/// smaller capacity at construction time rather than by hard-coded limits here.
class StorageServer : public Server {
public:
    using Server::Server;

    [[nodiscard]] std::string typeName() const override;

protected:
    /// Accepts only jobs that require at least some storage.
    [[nodiscard]] bool canRunJob(const ResourceRequirements& req) const noexcept override;
};
