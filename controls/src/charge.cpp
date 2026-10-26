#include "nutmeg/controls/charge.h"

namespace nutmeg::controls {

float chargePower(int heldTicks, const ChargeConfig& config)
{
    if (config.fullChargeTicks <= 0 || heldTicks >= config.fullChargeTicks) {
        return 1.0f;
    }
    if (heldTicks <= 0) {
        return config.minPower;
    }
    const float t = static_cast<float>(heldTicks) / static_cast<float>(config.fullChargeTicks);
    const float eased = t * t * (3.0f - 2.0f * t);
    return config.minPower + (1.0f - config.minPower) * eased;
}

}
