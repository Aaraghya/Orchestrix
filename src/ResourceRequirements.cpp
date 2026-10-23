#include "ResourceRequirements.h"

// ─── Private helper ───────────────────────────────────────────────────────────

void ResourceRequirements::validateField(double value, const std::string& name) {
    if (value < 0.0) {
        throw std::invalid_argument(
            "ResourceRequirements: " + name + " must be non-negative, got " +
            std::to_string(value));
    }
}

// ─── Constructor ──────────────────────────────────────────────────────────────

ResourceRequirements::ResourceRequirements(double cpu, double ram,
                                           double storage, double gpu)
    : cpu{cpu}, ram{ram}, storage{storage}, gpu{gpu}
{
    validateField(cpu,     "cpu");
    validateField(ram,     "ram");
    validateField(storage, "storage");
    validateField(gpu,     "gpu");
}

// ─── Methods ──────────────────────────────────────────────────────────────────

bool ResourceRequirements::fitsWithin(const ResourceRequirements& available) const noexcept {
    return cpu     <= available.cpu  &&
           ram     <= available.ram  &&
           storage <= available.storage &&
           gpu     <= available.gpu;
}

bool ResourceRequirements::isZero() const noexcept {
    return cpu == 0.0 && ram == 0.0 && storage == 0.0 && gpu == 0.0;
}
