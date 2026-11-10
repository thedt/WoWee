#pragma once

#include "rendering/animation/animation_ids.hpp"
#include <cstdint>

namespace wowee::core {

// Keep the packet callback and the per-frame sync on the same priority order.
[[nodiscard]] constexpr uint32_t movingCreatureAnimation(bool flying, bool swimming, bool walking) {
    if (flying) return rendering::anim::FLY_FORWARD;
    if (swimming) return rendering::anim::SWIM;
    return walking ? rendering::anim::WALK : rendering::anim::RUN;
}

// Some M2s have only one ground locomotion sequence. Do not turn a creature
// into a stationary default pose just because its preferred one is absent.
template <typename HasAnimation>
[[nodiscard]] uint32_t supportedCreatureAnimation(uint32_t preferred, HasAnimation hasAnimation) {
    if (preferred == rendering::anim::WALK && !hasAnimation(rendering::anim::WALK) &&
        hasAnimation(rendering::anim::RUN)) return rendering::anim::RUN;
    if (preferred == rendering::anim::RUN && !hasAnimation(rendering::anim::RUN) &&
        hasAnimation(rendering::anim::WALK)) return rendering::anim::WALK;
    return preferred;
}

} // namespace wowee::core
