#include "AppHdr.h"

#include "inventory-equipment.h"

#include "player.h"

namespace inventory_equipment
{
    void mark_weapon_changed(player &owner, int inventory_slot)
    {
        if (owner.equip[EQ_WEAPON] == inventory_slot)
            owner.wield_change = true;
    }

    void remove_stack(player &owner, int inventory_slot,
                      const std::function<void()> &on_unwield)
    {
        for (int i = EQ_FIRST_EQUIP; i < NUM_EQUIP; ++i)
        {
            if (owner.equip[i] == inventory_slot)
            {
                if (i == EQ_WEAPON)
                    on_unwield();
                owner.equip[i] = -1;
            }
        }
    }
}
