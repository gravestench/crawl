#pragma once

struct item_def;

namespace inventory_quantity
{
    enum class decrease_plan
    {
        partial,
        remove_stack
    };

    // Plan before equipment callbacks run; they may have other side effects.
    decrease_plan plan_decrease(const item_def &item, int amount);

    // Mutates only the item. Equipment, command, and quiver effects belong to
    // the caller and must happen in their existing order around this change.
    void apply_decrease(item_def &item, int amount, decrease_plan plan);
    void increase(item_def &item, int amount);
}
