#pragma once

#include <functional>

class game_state;
class player;
struct crawl_environment;

namespace level_item_quantity
{
    // The destroy callback must remove the item from the supplied level.
    // It runs before repeat/again cancellation, as in the legacy entry point.
    bool decrease(crawl_environment &level, game_state &commands,
                  const player &actor, int slot, int amount,
                  const std::function<void(int)> &destroy);
    void increase(crawl_environment &level, int slot, int amount);
}
