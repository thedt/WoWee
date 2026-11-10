#pragma once

#include <cmath>
#include <cstdint>
#include <optional>

namespace wowee::game {

// Wrath's monster spline carries a path and duration, but not necessarily the
// walk/run choice used to select its speed. This is an animation hint, not a
// movement flag: custom velocities can make the server's intent ambiguous.
[[nodiscard]] inline std::optional<bool> npcSplineWalkingHint(
        float pathLength, uint32_t durationMs, float walkSpeed, float runSpeed) {
    if (!std::isfinite(pathLength) || pathLength < 0.02f || durationMs == 0 ||
        !std::isfinite(walkSpeed) || !std::isfinite(runSpeed) ||
        walkSpeed <= 0.01f || runSpeed <= walkSpeed * 1.05f || runSpeed >= 200.0f) {
        return std::nullopt;
    }
    const float speed = pathLength * (1000.0f / static_cast<float>(durationMs));
    if (!std::isfinite(speed) || speed > 200.0f) return std::nullopt;

    // Path points can be quantized, and the length of a smooth curve is only
    // approximated by its control-point polyline. Allow some error, but do not
    // classify arbitrary scripted velocities as a normal walk or run. If both
    // ranges overlap, the packet doesn't give us enough information either.
    constexpr float kSpeedTolerance = 0.25f;
    const bool nearWalk = std::abs(speed - walkSpeed) <= walkSpeed * kSpeedTolerance;
    const bool nearRun = std::abs(speed - runSpeed) <= runSpeed * kSpeedTolerance;
    if (nearWalk == nearRun) return std::nullopt;
    return nearWalk;
}

} // namespace wowee::game
