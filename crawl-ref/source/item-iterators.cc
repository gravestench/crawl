#include "AppHdr.h"

#include "items.h"

#include "env.h"
#include "player.h"
#include "terrain.h"

/*---------------------------------------------------------------------*/
int visible_item_link(const crawl_environment &level, const coord_def& pos,
                      species_type player_species)
{
    return feat_eliminates_items(level.grid(pos), player_species)
           ? NON_ITEM : level.igrid(pos);
}

stack_iterator::stack_iterator(const coord_def& pos, bool accessible)
    : stack_iterator(env, accessible
                          ? visible_item_link(env, pos, you.species)
                          : env.igrid(pos))
{
}

stack_iterator::stack_iterator(int start_link)
    : stack_iterator(env, start_link)
{
}

stack_iterator::stack_iterator(crawl_environment &level_, const coord_def& pos)
    : stack_iterator(level_, level_.igrid(pos))
{
}

stack_iterator::stack_iterator(crawl_environment &level_, const coord_def& pos,
                               species_type player_species)
    : stack_iterator(level_, visible_item_link(level_, pos, player_species))
{
}

stack_iterator::stack_iterator(crawl_environment &level_, int start_link)
    : level(&level_)
{
    cur_link = start_link;
    if (cur_link != NON_ITEM)
        next_link = level->item[cur_link].link;
    else
        next_link = NON_ITEM;
}

stack_iterator::operator bool() const
{
    return cur_link != NON_ITEM;
}

item_def& stack_iterator::operator*() const
{
    ASSERT(cur_link != NON_ITEM);
    return level->item[cur_link];
}

item_def* stack_iterator::operator->() const
{
    ASSERT(cur_link != NON_ITEM);
    return &level->item[cur_link];
}

int stack_iterator::index() const
{
    return cur_link;
}

const stack_iterator& stack_iterator::operator ++ ()
{
    cur_link = next_link;
    if (cur_link != NON_ITEM)
        next_link = level->item[cur_link].link;
    return *this;
}

stack_iterator stack_iterator::operator++(int)
{
    const stack_iterator copy = *this;
    ++(*this);
    return copy;
}

bool stack_iterator::operator==(const stack_iterator& rhs) const
{
    return cur_link == rhs.cur_link
           && (cur_link == NON_ITEM || level == rhs.level);
}

mon_inv_iterator::mon_inv_iterator(monster& _mon)
    : mon_inv_iterator(env, _mon)
{
}

mon_inv_iterator::mon_inv_iterator(crawl_environment &level_, monster& _mon)
    : level(&level_), mon(_mon)
{
    type = static_cast<mon_inv_type>(0);
    if (mon.inv[type] == NON_ITEM)
        ++*this;
}

mon_inv_iterator::operator bool() const
{
    return type < NUM_MONSTER_SLOTS;
}

item_def& mon_inv_iterator::operator*() const
{
    ASSERT(mon.inv[type] != NON_ITEM);
    return level->item[mon.inv[type]];
}

item_def* mon_inv_iterator::operator->() const
{
    ASSERT(mon.inv[type] != NON_ITEM);
    return &level->item[mon.inv[type]];
}

mon_inv_iterator& mon_inv_iterator::operator ++ ()
{
    do
    {
        type = static_cast<mon_inv_type>(type + 1);
    }
    while (*this && mon.inv[type] == NON_ITEM);

    return *this;
}

mon_inv_iterator mon_inv_iterator::operator++(int)
{
    const mon_inv_iterator copy = *this;
    ++(*this);
    return copy;
}

