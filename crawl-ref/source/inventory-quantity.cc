#include "AppHdr.h"

#include "inventory-quantity.h"

#include "item-def.h"

namespace inventory_quantity
{
    decrease_plan plan_decrease(const item_def &item, int amount)
    {
        return item.quantity <= amount
               ? decrease_plan::remove_stack : decrease_plan::partial;
    }

    void apply_decrease(item_def &item, int amount, decrease_plan plan)
    {
        if (plan == decrease_plan::remove_stack)
        {
            // Match the inventory representation used by existing callers.
            // Other item metadata is deliberately left as it was.
            item.base_type = OBJ_UNASSIGNED;
            item.quantity = 0;
            item.props.clear();
            return;
        }

        item.quantity -= amount;
    }

    void increase(item_def &item, int amount)
    {
        item.quantity += amount;
    }
}
