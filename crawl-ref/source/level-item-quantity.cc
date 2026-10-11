#include "AppHdr.h"

#include "level-item-quantity.h"

#include "env.h"
#include "player.h"
#include "state.h"

namespace level_item_quantity
{
    bool decrease(crawl_environment &level, game_state &commands,
                  const player &actor, int slot, int amount,
                  const std::function<void(int)> &destroy)
    {
        item_def &item = level.item[slot];
        if (amount > item.quantity)
            amount = item.quantity;

        if (item.quantity == amount)
        {
            destroy(slot);
            // A repeated command must stop when its chosen stack disappears.
            commands.cancel_cmd_repeat(actor);
            commands.cancel_cmd_again(actor);
            return true;
        }

        item.quantity -= amount;
        return false;
    }

    void increase(crawl_environment &level, int slot, int amount)
    {
        level.item[slot].quantity += amount;
        ASSERT(level.item[slot].defined());
    }
}
