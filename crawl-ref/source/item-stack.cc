#include "AppHdr.h"

#include "item-stack.h"

#include <limits>

#include "artefact.h"
#include "item-def.h"
#include "item-prop.h"
#include "item-status-flag-type.h"
#include "tag-version.h"

bool is_stackable_item(const item_def &item)
{
    if (!item.defined())
        return false;

    switch (item.base_type)
    {
        case OBJ_MISSILES:
        case OBJ_SCROLLS:
        case OBJ_POTIONS:
        case OBJ_GOLD:
#if TAG_MAJOR_VERSION == 34
        case OBJ_FOOD:
#endif
            return true;
        case OBJ_MISCELLANY:
            switch (item.sub_type)
            {
                case MISC_ZIGGURAT:
#if TAG_MAJOR_VERSION == 34
                case MISC_SACK_OF_SPIDERS:
#endif
                    return true;
                default:
                    break;
            }
        default:
            break;
    }
    return false;
}

bool items_similar(const item_def &item1, const item_def &item2)
{
    // Base and sub-types must always be the same to stack.
    if (item1.base_type != item2.base_type || item1.sub_type != item2.sub_type)
        return false;

    if (item1.base_type == OBJ_GOLD || item1.base_type == OBJ_RUNES)
        return true;

    if (is_artefact(item1) != is_artefact(item2))
        return false;
    else if (is_artefact(item1)
             && get_artefact_name(item1) != get_artefact_name(item2))
    {
        return false;
    }

    // Missiles with different egos shouldn't merge.
    if (item1.base_type == OBJ_MISSILES && item1.brand != item2.brand)
        return false;

    // Don't merge trapping nets with other nets.
    if (item1.is_type(OBJ_MISSILES, MI_THROWING_NET)
        && item1.net_placed != item2.net_placed)
    {
        return false;
    }

    const iflags_t no_merge_flags = ISFLAG_MIMIC | ISFLAG_SUMMONED;
    if ((item1.flags & no_merge_flags) != (item2.flags & no_merge_flags))
        return false;

    // The inscriptions can differ if one of them is blank, but if they
    // are differing non-blank inscriptions then don't stack.
    if (item1.inscription != item2.inscription
        && !item1.inscription.empty() && !item2.inscription.empty())
    {
        return false;
    }

    return true;
}

static bool _eligible_stack_pair(const item_def &item1,
                                 const item_def &item2)
{
    // Both items must be stackable.
    if (!is_stackable_item(item1) || !is_stackable_item(item2)
        || static_cast<long long>(item1.quantity) + item2.quantity
            > std::numeric_limits<decltype(item1.quantity)>::max())
    {
        return false;
    }

    return items_similar(item1, item2);
}

bool items_stack(const item_def &item1, const item_def &item2,
                 bool first_identified, bool second_identified)
{
    // Don't leak information when checking if an "(unknown)" shop item
    // matches an unidentified item in inventory.
    return _eligible_stack_pair(item1, item2)
           && first_identified == second_identified;
}

bool items_stack(const item_def &item1, const item_def &item2)
{
    // Keep the inexpensive eligibility checks ahead of identification. The
    // knowledge lookup can depend on the current game's item type state.
    if (!_eligible_stack_pair(item1, item2))
        return false;

    return items_stack(item1, item2,
                       fully_identified(item1), fully_identified(item2));
}
