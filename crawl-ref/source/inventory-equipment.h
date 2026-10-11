#pragma once

#include <functional>

class player;

namespace inventory_equipment
{
    // Mark a changed weapon stack before its quantity is modified.
    void mark_weapon_changed(player &owner, int inventory_slot);

    // Clear references to a consumed stack. The weapon callback runs while
    // the weapon is still equipped, as required by unwield_item().
    void remove_stack(player &owner, int inventory_slot,
                      const std::function<void()> &on_unwield);
}
