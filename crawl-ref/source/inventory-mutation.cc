#include "AppHdr.h"

#include "inventory-mutation.h"

#include "inventory-equipment.h"
#include "inventory-quantity.h"
#include "player.h"
#include "state.h"

namespace inventory_mutation
{
    bool decrease(player &owner, game_state &commands, int slot, int amount,
                  const effects &callbacks)
    {
        item_def &item = owner.inv[slot];
        const auto plan = inventory_quantity::plan_decrease(item, amount);
        const bool stack_removed =
            plan == inventory_quantity::decrease_plan::remove_stack;

        inventory_equipment::mark_weapon_changed(owner, slot);

        if (stack_removed)
        {
            inventory_equipment::remove_stack(owner, slot, callbacks.unwield);
            callbacks.hide_skills(item);
        }

        inventory_quantity::apply_decrease(item, amount, plan);

        if (stack_removed)
        {
            commands.cancel_cmd_repeat(owner);
            commands.cancel_cmd_again(owner);
            callbacks.quiver_changed(false);
        }

        return stack_removed;
    }

    void increase(player &owner, int slot, int amount,
                  const effects &callbacks)
    {
        inventory_equipment::mark_weapon_changed(owner, slot);
        inventory_quantity::increase(owner.inv[slot], amount);
        if (owner.inv[slot].quantity == amount)
            callbacks.quiver_changed(true);
    }
}
