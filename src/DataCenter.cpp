#include "DataCenter.h"
#include <algorithm>
#include <stdexcept>

void DataCenter::addServer(std::unique_ptr<Server> server) {
    if (!server) {
        throw std::invalid_argument("DataCenter::addServer: server pointer must not be null.");
    }

    const std::string& id = server->getId();

    // Check for duplicate IDs.
    const bool duplicate = std::any_of(
        m_servers.begin(), m_servers.end(),
        [&id](const std::unique_ptr<Server>& s) { return s->getId() == id; });

    if (duplicate) {
        throw std::invalid_argument(
            "DataCenter::addServer: a server with id '" + id + "' already exists.");
    }

    m_servers.push_back(std::move(server));
}

Server* DataCenter::findServer(const std::string& id) const noexcept {
    for (const auto& s : m_servers) {
        if (s->getId() == id) {
            return s.get();
        }
    }
    return nullptr;
}

const std::vector<std::unique_ptr<Server>>& DataCenter::getServers() const noexcept {
    return m_servers;
}

std::size_t DataCenter::serverCount() const noexcept {
    return m_servers.size();
}
