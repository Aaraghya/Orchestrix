#pragma once

#include "Server.h"
#include <memory>
#include <string>
#include <vector>
#include <stdexcept>

/// Owns and provides access to all servers in the simulated data center.
///
/// Design decisions:
///  - Servers are stored as unique_ptr<Server> so the DataCenter owns them.
///    Callers receive raw (non-owning) pointers for inspection and operations.
///  - Server IDs must be unique; addServer() throws std::invalid_argument on
///    duplicates (architecture.md §3.4, simulation-rules.md §4).
///  - The server collection is exposed as a const reference to a vector of
///    unique_ptr so that the future Scheduler can iterate it in O(n) without
///    additional copies.  The returned reference stays valid as long as no
///    server is added or removed.
///  - There is intentionally no removeServer() in Milestone 1.  Failure
///    handling (which would need server removal/marking) is an optional
///    extension defined in §13 of simulation-rules.md.
class DataCenter {
public:
    DataCenter() = default;

    // Non-copyable (owns unique resources), movable.
    DataCenter(const DataCenter&)            = delete;
    DataCenter& operator=(const DataCenter&) = delete;
    DataCenter(DataCenter&&)                 = default;
    DataCenter& operator=(DataCenter&&)      = default;

    /// Adds a server to the data center.
    ///
    /// Throws std::invalid_argument when:
    ///   - \p server is null.
    ///   - A server with the same ID already exists.
    void addServer(std::unique_ptr<Server> server);

    /// Returns a non-owning pointer to the server with \p id, or nullptr if
    /// no such server exists.
    [[nodiscard]] Server* findServer(const std::string& id) const noexcept;

    /// Returns a reference to the internal server collection.
    ///
    /// The reference is valid until the next call to addServer().
    /// Use this to iterate all servers (e.g. in the Scheduler).
    [[nodiscard]] const std::vector<std::unique_ptr<Server>>& getServers() const noexcept;

    /// Returns the total number of registered servers.
    [[nodiscard]] std::size_t serverCount() const noexcept;

private:
    std::vector<std::unique_ptr<Server>> m_servers;
};
