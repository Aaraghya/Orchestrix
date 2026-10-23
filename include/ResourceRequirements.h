#pragma once

#include <stdexcept>
#include <string>

/// Represents the resource requirements of a computational job.
///
/// All quantities are non-negative. Attempting to construct a
/// ResourceRequirements object with any negative value throws
/// std::invalid_argument so that invalid data cannot propagate
/// further into the system.
///
/// Units:
///   cpu     – number of logical CPU cores  (integer-valued double)
///   ram     – RAM in GB                    (fractional values allowed)
///   storage – storage in GB               (fractional values allowed)
///   gpu     – number of GPU devices        (integer-valued double)
struct ResourceRequirements {
    double cpu     = 0.0;   ///< CPU cores required.
    double ram     = 0.0;   ///< RAM in GB required.
    double storage = 0.0;   ///< Storage in GB required.
    double gpu     = 0.0;   ///< GPU devices required.

    /// Constructs a ResourceRequirements and validates that every field is
    /// non-negative.  Throws std::invalid_argument on violation.
    ResourceRequirements(double cpu, double ram, double storage, double gpu);

    /// Default constructor: all resources are zero (valid).
    ResourceRequirements() = default;

    /// Returns true when every field of *this is <= the corresponding field
    /// of \p available, meaning this request fits inside the available pool.
    [[nodiscard]] bool fitsWithin(const ResourceRequirements& available) const noexcept;

    /// Returns true when every field is exactly zero.
    [[nodiscard]] bool isZero() const noexcept;

private:
    static void validateField(double value, const std::string& name);
};
