#pragma once

namespace nutmeg::controls {

struct ChargeConfig {
    int fullChargeTicks = 45;
    float minPower = 0.2f;
};

// Power in [minPower, 1] for a button held this many ticks: a tap is a soft
// touch, a full hold is a full-power strike, with a smoothstep in between.
float chargePower(int heldTicks, const ChargeConfig& config = {});

}
