#pragma once

#include <functional>

class game_state;
class player;
struct item_def;

namespace inventory_mutation
{
    struct effects
    {
        // Unwield must run while the weapon still occupies its equipment slot.
        std::function<void()> unwield;
        std::function<void(const item_def &)> hide_skills;
        std::function<void(bool)> quiver_changed;
    };

    bool decrease(player &owner, game_state &commands, int slot, int amount,
                  const effects &callbacks);
    void increase(player &owner, int slot, int amount,
                  const effects &callbacks);
}
