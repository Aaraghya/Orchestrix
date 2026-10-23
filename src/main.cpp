/// main.cpp — Milestone 1 smoke test
///
/// Demonstrates:
///  1. Building a small DataCenter with three different server types.
///  2. Creating a job with well-defined resource requirements.
///  3. Checking resource feasibility with canAccommodate().
///  4. Performing a reservation and verifying the accounting invariants.
///  5. Releasing the reservation and confirming resources are restored.
///
/// This is intentionally minimal; it is NOT the final CLI.

#include "DataCenter.h"
#include "Job.h"
#include "ResourceRequirements.h"
#include "Server.h"

#include <iostream>
#include <memory>

/// Print a summary line for a server.
static void printServer(const Server& s) {
    const auto& cap   = s.getCapacity();
    const auto  alloc = s.getAllocated();
    const auto  avail = s.getAvailable();

    std::cout << "  [" << s.typeName() << "] id=" << s.getId() << "\n"
              << "    capacity : cpu=" << cap.cpu   << " ram=" << cap.ram
              << " storage=" << cap.storage << " gpu=" << cap.gpu << "\n"
              << "    allocated: cpu=" << alloc.cpu << " ram=" << alloc.ram
              << " storage=" << alloc.storage << " gpu=" << alloc.gpu << "\n"
              << "    available: cpu=" << avail.cpu << " ram=" << avail.ram
              << " storage=" << avail.storage << " gpu=" << avail.gpu << "\n";
}

int main() {
    std::cout << "=== Orchestrix Milestone 1 — Smoke Test ===\n\n";

    // ── Build the data center ──────────────────────────────────────────────

    DataCenter dc;

    // ComputeServer: 16 CPU, 64 GB RAM, 0 GPU, 500 GB storage.
    dc.addServer(std::make_unique<ComputeServer>(
        "compute-01",
        ResourceRequirements{16.0, 64.0, 500.0, 0.0}));

    // GPUServer: 8 CPU, 32 GB RAM, 4 GPUs, 200 GB storage.
    dc.addServer(std::make_unique<GPUServer>(
        "gpu-01",
        ResourceRequirements{8.0, 32.0, 200.0, 4.0}));

    // StorageServer: 4 CPU, 16 GB RAM, 0 GPUs, 10000 GB storage.
    dc.addServer(std::make_unique<StorageServer>(
        "storage-01",
        ResourceRequirements{4.0, 16.0, 10000.0, 0.0}));

    std::cout << "Data center initialised with " << dc.serverCount() << " servers:\n";
    for (const auto& srv : dc.getServers()) {
        printServer(*srv);
    }

    // ── Create a job ───────────────────────────────────────────────────────

    // A GPU workload: needs 4 CPU cores, 8 GB RAM, 50 GB storage, 1 GPU.
    Job gpuJob{
        "job-gpu-001",
        ResourceRequirements{4.0, 8.0, 50.0, 1.0},
        /*priority=*/   5,
        /*arrivalTick=*/0,
        /*duration=*/   3
    };

    std::cout << "\nJob '" << gpuJob.getId() << "' created.\n"
              << "  state=" << jobStateToString(gpuJob.getState())
              << " arrival=" << gpuJob.getArrivalTick()
              << " duration=" << gpuJob.getDuration() << "\n";

    // ── Check feasibility ──────────────────────────────────────────────────

    std::cout << "\nFeasibility check for '" << gpuJob.getId() << "':\n";
    for (const auto& srv : dc.getServers()) {
        const bool fits = srv->canAccommodate(gpuJob.getRequirements());
        std::cout << "  " << srv->getId() << " (" << srv->typeName() << "): "
                  << (fits ? "CAN accommodate" : "CANNOT accommodate") << "\n";
    }

    // ── Reserve resources on the GPU server ───────────────────────────────

    Server* gpuServer = dc.findServer("gpu-01");
    if (!gpuServer) {
        std::cerr << "ERROR: gpu-01 not found.\n";
        return 1;
    }

    std::cout << "\nReserving resources for '" << gpuJob.getId()
              << "' on '" << gpuServer->getId() << "'...\n";
    gpuServer->reserveResources(gpuJob.getRequirements());

    // Simulate the job starting.
    gpuJob.arrive();   // PENDING → QUEUED
    gpuJob.start(gpuServer->getId(), /*startTick=*/0); // QUEUED → RUNNING
    std::cout << "  Job state: " << jobStateToString(gpuJob.getState()) << "\n";

    std::cout << "  After reservation:\n";
    printServer(*gpuServer);

    // ── Release resources ──────────────────────────────────────────────────

    std::cout << "Releasing resources (simulating job completion at tick 3)...\n";
    gpuJob.complete(/*finishTick=*/3);
    gpuServer->releaseResources(gpuJob.getRequirements());

    std::cout << "  Job state: " << jobStateToString(gpuJob.getState()) << "\n";
    std::cout << "  After release:\n";
    printServer(*gpuServer);

    // ── Verify accounting invariants ───────────────────────────────────────

    const auto avail = gpuServer->getAvailable();
    const auto& cap  = gpuServer->getCapacity();
    const bool ok =
        avail.cpu     == cap.cpu     &&
        avail.ram     == cap.ram     &&
        avail.storage == cap.storage &&
        avail.gpu     == cap.gpu;

    std::cout << "\nAccounting invariant (available == capacity): "
              << (ok ? "PASS" : "FAIL") << "\n";

    // ── Demonstrate duplicate server ID rejection ──────────────────────────

    std::cout << "\nAttempting to add duplicate server 'gpu-01'...\n";
    try {
        dc.addServer(std::make_unique<GPUServer>(
            "gpu-01",
            ResourceRequirements{4.0, 16.0, 100.0, 2.0}));
        std::cout << "  ERROR: duplicate was not rejected.\n";
    } catch (const std::invalid_argument& e) {
        std::cout << "  Correctly rejected: " << e.what() << "\n";
    }

    std::cout << "\n=== Smoke test complete. ===\n";
    return 0;
}
