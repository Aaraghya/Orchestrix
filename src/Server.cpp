#include "Server.h"
#include <stdexcept>
#include <string>

// ─── Server ───────────────────────────────────────────────────────────────────

Server::Server(std::string id, ResourceRequirements capacity)
    : m_id{std::move(id)}
    , m_capacity{std::move(capacity)}
    , m_allocated{} // zero-initialised by default ResourceRequirements ctor
    , m_available{true}
{
    if (m_id.empty()) {
        throw std::invalid_argument("Server: id must not be empty.");
    }
    // capacity validation is handled by ResourceRequirements constructor.
}

// ── Accessors ─────────────────────────────────────────────────────────────────

const std::string& Server::getId() const noexcept { return m_id; }

const ResourceRequirements& Server::getCapacity() const noexcept {
    return m_capacity;
}

ResourceRequirements Server::getAllocated() const noexcept {
    return m_allocated;
}

ResourceRequirements Server::getAvailable() const noexcept {
    // Derived on demand — avoids the two-field drift problem.
    return ResourceRequirements{
        m_capacity.cpu     - m_allocated.cpu,
        m_capacity.ram     - m_allocated.ram,
        m_capacity.storage - m_allocated.storage,
        m_capacity.gpu     - m_allocated.gpu
    };
}

bool Server::isAvailable() const noexcept {
    return m_available;
}

// ── Resource operations ───────────────────────────────────────────────────────

bool Server::canAccommodate(const ResourceRequirements& req) const noexcept {
    if (!m_available) {
        return false;
    }
    if (!canRunJob(req)) {
        return false;
    }
    return req.fitsWithin(getAvailable());
}

void Server::reserveResources(const ResourceRequirements& req) {
    if (!m_available) {
        throw std::runtime_error(
            "Server '" + m_id + "': cannot reserve resources - server is unavailable.");
    }

    if (!canRunJob(req)) {
        throw std::runtime_error(
            "Server '" + m_id + "': workload type is not compatible with this server (" +
            typeName() + ").");
    }

    // Check feasibility before modifying anything (atomicity guarantee).
    const ResourceRequirements avail = getAvailable();
    if (!req.fitsWithin(avail)) {
        throw std::runtime_error(
            "Server '" + m_id + "': insufficient resources. "
            "Requested cpu=" + std::to_string(req.cpu) +
            " ram="     + std::to_string(req.ram) +
            " storage=" + std::to_string(req.storage) +
            " gpu="     + std::to_string(req.gpu) +
            "; available cpu=" + std::to_string(avail.cpu) +
            " ram="     + std::to_string(avail.ram) +
            " storage=" + std::to_string(avail.storage) +
            " gpu="     + std::to_string(avail.gpu) + ".");
    }

    // Commit the reservation only after all checks passed.
    m_allocated.cpu     += req.cpu;
    m_allocated.ram     += req.ram;
    m_allocated.storage += req.storage;
    m_allocated.gpu     += req.gpu;
}

void Server::releaseResources(const ResourceRequirements& req) {
    // Verify that releasing would not make any allocated dimension go negative.
    // This catches double-releases and mis-matched release amounts.
    if (req.cpu     > m_allocated.cpu  ||
        req.ram     > m_allocated.ram  ||
        req.storage > m_allocated.storage ||
        req.gpu     > m_allocated.gpu) {
        throw std::runtime_error(
            "Server '" + m_id + "': cannot release more resources than are allocated. "
            "Release cpu=" + std::to_string(req.cpu) +
            " ram="     + std::to_string(req.ram) +
            " storage=" + std::to_string(req.storage) +
            " gpu="     + std::to_string(req.gpu) +
            "; allocated cpu=" + std::to_string(m_allocated.cpu) +
            " ram="     + std::to_string(m_allocated.ram) +
            " storage=" + std::to_string(m_allocated.storage) +
            " gpu="     + std::to_string(m_allocated.gpu) + ".");
    }

    m_allocated.cpu     -= req.cpu;
    m_allocated.ram     -= req.ram;
    m_allocated.storage -= req.storage;
    m_allocated.gpu     -= req.gpu;
}

void Server::setAvailable(bool available) noexcept {
    m_available = available;
}

bool Server::isCompatibleWith(const ResourceRequirements& req) const noexcept {
    return canRunJob(req);
}

// ─── ComputeServer ────────────────────────────────────────────────────────────

std::string ComputeServer::typeName() const { return "ComputeServer"; }

bool ComputeServer::canRunJob(const ResourceRequirements& /*req*/) const noexcept {
    // ComputeServer accepts all workloads; resource-fit is enforced by the
    // base class through fitsWithin().
    return true;
}

// ─── GPUServer ────────────────────────────────────────────────────────────────

std::string GPUServer::typeName() const { return "GPUServer"; }

bool GPUServer::canRunJob(const ResourceRequirements& req) const noexcept {
    // Only accept jobs that actually need GPU resources.
    // This prevents non-GPU workloads from occupying a scarce specialised server.
    return req.gpu > 0.0;
}

// ─── StorageServer ────────────────────────────────────────────────────────────

std::string StorageServer::typeName() const { return "StorageServer"; }

bool StorageServer::canRunJob(const ResourceRequirements& req) const noexcept {
    // Only accept jobs that request storage.
    // This keeps the specialised high-storage pool available for I/O-bound workloads.
    return req.storage > 0.0;
}
