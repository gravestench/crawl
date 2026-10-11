#include "catch.hpp"

#include "AppHdr.h"

#include <memory>

#include "artefact.h"
#include "art-enum.h"
#include "dungeon.h"
#include "env.h"
#include "items.h"
#include "item-prop.h"
#include "item-prop-enum.h"
#include "item-status-flag-type.h"
#include "item-stack.h"
#include "invent.h"
#include "inventory-equipment.h"
#include "inventory-mutation.h"
#include "inventory-quantity.h"
#include "level-item-quantity.h"
#include "player.h"
#include "player-equip.h"
#include "potion-type.h"
#include "state.h"

#include "test_player_fixture.h"

TEST_CASE("Partial inventory consumption preserves the stack", "[inventory]")
{
    item_def item;
    item.base_type = OBJ_SCROLLS;
    item.quantity = 5;
    item.inscription = "keep";

    const auto plan = inventory_quantity::plan_decrease(item, 2);
    REQUIRE(plan == inventory_quantity::decrease_plan::partial);
    inventory_quantity::apply_decrease(item, 2, plan);
    REQUIRE(item.base_type == OBJ_SCROLLS);
    REQUIRE(item.quantity == 3);
    REQUIRE(item.inscription == "keep");
}

TEST_CASE("Consuming an inventory stack preserves legacy slot metadata",
          "[inventory]")
{
    item_def item;
    item.base_type = OBJ_SCROLLS;
    item.quantity = 2;
    item.inscription = "keep";
    item.link = 7;
    item.props["test"] = 1;

    const auto plan = inventory_quantity::plan_decrease(item, 2);
    REQUIRE(plan == inventory_quantity::decrease_plan::remove_stack);
    inventory_quantity::apply_decrease(item, 2, plan);
    REQUIRE(item.base_type == OBJ_UNASSIGNED);
    REQUIRE(item.quantity == 0);
    REQUIRE(item.inscription == "keep");
    REQUIRE(item.link == 7);
    REQUIRE(item.props.empty());
}

TEST_CASE("Inventory consumption decision survives equipment callbacks",
          "[inventory]")
{
    item_def item;
    item.base_type = OBJ_SCROLLS;
    item.quantity = 2;

    const auto plan = inventory_quantity::plan_decrease(item, 2);
    item.quantity = 3; // A callback can change state before consumption runs.
    inventory_quantity::apply_decrease(item, 2, plan);

    REQUIRE(item.base_type == OBJ_UNASSIGNED);
    REQUIRE(item.quantity == 0);
}

TEST_CASE("Inventory equipment cleanup keeps the weapon callback order",
          "[inventory]")
{
    player owner;
    owner.equip[EQ_WEAPON] = 7;
    owner.equip[EQ_CLOAK] = 7;
    owner.equip[EQ_HELMET] = 8;

    inventory_equipment::mark_weapon_changed(owner, 7);
    REQUIRE(owner.wield_change);

    int unwield_calls = 0;
    inventory_equipment::remove_stack(owner, 7, [&] {
        ++unwield_calls;
        REQUIRE(owner.equip[EQ_WEAPON] == 7);
        REQUIRE(owner.equip[EQ_CLOAK] == 7);
    });

    REQUIRE(unwield_calls == 1);
    REQUIRE(owner.equip[EQ_WEAPON] == -1);
    REQUIRE(owner.equip[EQ_CLOAK] == -1);
    REQUIRE(owner.equip[EQ_HELMET] == 8);
}

TEST_CASE("Inventory equipment cleanup leaves unrelated stacks alone",
          "[inventory]")
{
    player owner;
    owner.equip[EQ_WEAPON] = 3;
    owner.equip[EQ_SHIELD] = 5;

    inventory_equipment::mark_weapon_changed(owner, 5);
    REQUIRE_FALSE(owner.wield_change);

    inventory_equipment::remove_stack(owner, 5, [] { FAIL("not wielded"); });
    REQUIRE(owner.equip[EQ_WEAPON] == 3);
    REQUIRE(owner.equip[EQ_SHIELD] == -1);
}

TEST_CASE("Inventory removal orders equipment and skill effects before clearing",
          "[inventory]")
{
    player owner;
    game_state commands;
    owner.inv[4].base_type = OBJ_WEAPONS;
    owner.inv[4].quantity = 1;
    owner.equip[EQ_WEAPON] = 4;

    int effects_seen = 0;
    const bool removed = inventory_mutation::decrease(owner, commands, 4, 1, {
        [&] {
            REQUIRE(owner.equip[EQ_WEAPON] == 4);
            REQUIRE(owner.inv[4].quantity == 1);
            effects_seen = 1;
        },
        [&](const item_def &item) {
            REQUIRE(effects_seen == 1);
            REQUIRE(item.quantity == 1);
            REQUIRE(owner.equip[EQ_WEAPON] == -1);
            effects_seen = 2;
        },
        [&](bool autoswitch) {
            REQUIRE_FALSE(autoswitch);
            REQUIRE(effects_seen == 2);
            REQUIRE(owner.inv[4].quantity == 0);
            effects_seen = 3;
        }
    });

    REQUIRE(removed);
    REQUIRE(owner.wield_change);
    REQUIRE(effects_seen == 3);
}

TEST_CASE("Inventory increase refreshes quiver only for a new stack",
          "[inventory]")
{
    player owner;
    owner.inv[4].base_type = OBJ_SCROLLS;
    int refreshes = 0;
    const inventory_mutation::effects callbacks = {
        {}, {}, [&](bool autoswitch) {
            REQUIRE(autoswitch);
            ++refreshes;
        }
    };

    inventory_mutation::increase(owner, 4, 2, callbacks);
    REQUIRE(owner.inv[4].quantity == 2);
    REQUIRE(refreshes == 1);

    inventory_mutation::increase(owner, 4, 1, callbacks);
    REQUIRE(owner.inv[4].quantity == 3);
    REQUIRE(refreshes == 1);
}

TEST_CASE("Consuming a stack cancels repeat and redo on its command state",
          "[inventory][state]")
{
    player owner;
    game_state commands;
    owner.inv[4].base_type = OBJ_SCROLLS;
    owner.inv[4].quantity = 1;
    commands.repeat_cmd = CMD_WAIT;
    commands.doing_prev_cmd_again = true;

    REQUIRE(inventory_mutation::decrease(owner, commands, 4, 1, {
        [] {}, [](const item_def &) {}, [](bool) {}
    }));
    REQUIRE(commands.repeat_cmd == CMD_NO_CMD);
    REQUIRE_FALSE(commands.doing_prev_cmd_again);
}

TEST_CASE("Floor quantity updates affect only the supplied level", "[inventory]")
{
    std::unique_ptr<crawl_environment> first(new crawl_environment);
    std::unique_ptr<crawl_environment> second(new crawl_environment);
    first->item[0].base_type = OBJ_SCROLLS;
    first->item[0].quantity = 2;
    second->item[0].base_type = OBJ_SCROLLS;
    second->item[0].quantity = 7;

    inc_mitm_item_quantity(*first, 0, 3);

    REQUIRE(first->item[0].quantity == 5);
    REQUIRE(second->item[0].quantity == 7);
}

TEST_CASE("Floor stack removal uses the supplied level and destroys first",
          "[inventory]")
{
    std::unique_ptr<crawl_environment> level(new crawl_environment);
    game_state commands;
    player actor;
    level->item[3].base_type = OBJ_SCROLLS;
    level->item[3].quantity = 2;
    int destroyed_slot = -1;

    const bool removed = level_item_quantity::decrease(
        *level, commands, actor, 3, 5, [&](int slot) {
            REQUIRE(slot == 3);
            REQUIRE(level->item[slot].quantity == 2);
            destroyed_slot = slot;
            level->item[slot].clear();
        });

    REQUIRE(removed);
    REQUIRE(destroyed_slot == 3);
    REQUIRE_FALSE(level->item[3].defined());
}

TEST_CASE("Partial floor consumption leaves destruction alone", "[inventory]")
{
    std::unique_ptr<crawl_environment> level(new crawl_environment);
    game_state commands;
    player actor;
    level->item[3].base_type = OBJ_SCROLLS;
    level->item[3].quantity = 5;

    const bool removed = level_item_quantity::decrease(
        *level, commands, actor, 3, 2, [](int) { FAIL("partial stack"); });

    REQUIRE_FALSE(removed);
    REQUIRE(level->item[3].quantity == 3);
}

TEST_CASE("Floor stack removal cancels repeat and redo on its command state",
          "[inventory][state]")
{
    std::unique_ptr<crawl_environment> level(new crawl_environment);
    game_state commands;
    player actor;
    level->item[3].base_type = OBJ_SCROLLS;
    level->item[3].quantity = 1;
    commands.repeat_cmd = CMD_WAIT;
    commands.doing_prev_cmd_again = true;

    REQUIRE(level_item_quantity::decrease(
        *level, commands, actor, 3, 1,
        [&](int slot) { level->item[slot].clear(); }));
    REQUIRE(commands.repeat_cmd == CMD_NO_CMD);
    REQUIRE_FALSE(commands.doing_prev_cmd_again);
}

TEST_CASE("Item stack rules distinguish inscriptions and summoned items",
          "[inventory]")
{
    item_def first;
    first.base_type = OBJ_SCROLLS;
    first.sub_type = SCR_FEAR;
    first.quantity = 2;
    item_def second = first;

    REQUIRE(is_stackable_item(first));
    REQUIRE(items_similar(first, second));

    first.inscription = "keep";
    REQUIRE(items_similar(first, second));
    second.inscription = "drop";
    REQUIRE_FALSE(items_similar(first, second));

    second.inscription = "keep";
    second.flags |= ISFLAG_SUMMONED;
    REQUIRE_FALSE(items_similar(first, second));
}

TEST_CASE("Stack compatibility uses supplied identification knowledge",
          "[inventory]")
{
    item_def first;
    first.base_type = OBJ_SCROLLS;
    first.sub_type = SCR_FEAR;
    first.quantity = 2;
    item_def second = first;

    REQUIRE(items_stack(first, second, false, false));
    REQUIRE(items_stack(first, second, true, true));
    REQUIRE_FALSE(items_stack(first, second, true, false));
    REQUIRE_FALSE(items_stack(first, second, false, true));
}

TEST_CASE("Item iterators read the supplied level's item storage",
          "[inventory]")
{
    std::unique_ptr<crawl_environment> level(new crawl_environment);
    const coord_def floor_pos(5, 5);
    level->igrid(floor_pos) = 0;
    level->item[0].base_type = OBJ_SCROLLS;
    level->item[0].link = 1;
    level->item[1].base_type = OBJ_WEAPONS;
    level->item[1].link = NON_ITEM;
    level->grid(floor_pos) = DNGN_FLOOR;

    REQUIRE(visible_item_link(*level, floor_pos, SP_HUMAN) == 0);

    stack_iterator floor(*level, floor_pos);
    REQUIRE(floor.index() == 0);
    REQUIRE(floor->base_type == OBJ_SCROLLS);
    ++floor;
    REQUIRE(floor.index() == 1);
    REQUIRE(floor->base_type == OBJ_WEAPONS);
    ++floor;
    REQUIRE(floor == stack_iterator::end(*level));

    level->grid(floor_pos) = DNGN_DEEP_WATER;
    REQUIRE(visible_item_link(*level, floor_pos, SP_HUMAN) == NON_ITEM);
    REQUIRE(visible_item_link(*level, floor_pos, SP_MERFOLK) == 0);
    REQUIRE_FALSE(stack_iterator(*level, floor_pos, SP_HUMAN));
    REQUIRE(stack_iterator(*level, floor_pos, SP_MERFOLK));

    level->grid(floor_pos) = DNGN_LAVA;
    REQUIRE(visible_item_link(*level, floor_pos, SP_MERFOLK) == NON_ITEM);

    monster carrier;
    carrier.inv[MSLOT_WEAPON] = 1;
    mon_inv_iterator gear(*level, carrier);
    REQUIRE(gear.slot() == MSLOT_WEAPON);
    REQUIRE(gear->base_type == OBJ_WEAPONS);
    ++gear;
    REQUIRE_FALSE(gear);
}

TEST_CASE( "Create item def", "[single-file]" ) {
    item_def foo;
}

TEST_CASE( "Create a specific item def", "[single-file]" ) {
    item_def scroll_of_fear;

    get_item_by_exact_name(scroll_of_fear, "scroll of fear");

    REQUIRE(scroll_of_fear.base_type == OBJ_SCROLLS);
    REQUIRE(scroll_of_fear.sub_type == SCR_FEAR);
}

TEST_CASE_METHOD( MockPlayerYouTestsFixture,
                  "Check mock player has no items", "[single-file]" ) {
    for (int i=0; i < ENDOFPACK; i++)
        REQUIRE(you.inv[i].base_type == OBJ_UNASSIGNED);

    REQUIRE(!any_items_of_type(OSEL_ANY));

}

static item_def simple_create_item(object_class_type base_type,
                                   int sub_type, int plus = 0,
                                   special_armour_type ego = SPARM_NORMAL);

static item_def simple_create_item(object_class_type base_type,
                                   int sub_type, int plus,
                                   special_armour_type ego){
    item_def item;

    item.clear();

    item.base_type = base_type;
    item.sub_type = sub_type;
    item.quantity = 1;
    item.plus = plus;
    item.brand = ego;

    return item;
}

TEST_CASE_METHOD(MockPlayerYouTestsFixture,
                 "Consuming an equipped weapon clears its inventory slot",
                 "[inventory]")
{
    you.inv[0] = simple_create_item(OBJ_WEAPONS, WPN_DAGGER);
    you.inv[0].link = 0;
    equip_item(EQ_WEAPON, 0, false, true);

    REQUIRE(dec_inv_item_quantity(0, 1));
    REQUIRE(you.inv[0].base_type == OBJ_UNASSIGNED);
    REQUIRE(you.inv[0].quantity == 0);
    REQUIRE(you.equip[EQ_WEAPON] == -1);
    REQUIRE(you.wield_change);
}

static int find_inv_index_with_exact_item(object_class_type base_type, int sub_type){

    for (int i=0; i < ENDOFPACK; i++)
    {
        item_def item = you.inv[i];
        if (item.base_type == base_type
               && item.sub_type == sub_type)
        {
            return i;
        }
    }

    return -1;
}

TEST_CASE_METHOD( MockPlayerYouTestsFixture,
                  "Give mock player item", "[single-file]" ) {
    item_def scroll_of_fear;

    get_item_by_exact_name(scroll_of_fear, "scroll of fear");

    move_item_to_inv(scroll_of_fear);

    REQUIRE(any_items_of_type(OSEL_ANY));

    int num_fear_stacks_in_invent = 0;

    for (int i=0; i < ENDOFPACK; i++)
    {
        item_def item = you.inv[i];
        if (item.base_type == OBJ_SCROLLS
               && item.sub_type == SCR_FEAR)
        {
            num_fear_stacks_in_invent++;
        }
    }

    REQUIRE(num_fear_stacks_in_invent == 1);
}

TEST_CASE_METHOD( MockPlayerYouTestsFixture,
                  "Mock player armour skill", "[single-file]" ) {
    REQUIRE(you.skill(SK_ARMOUR,20) == 0);
    REQUIRE(you.strength() == 11);
}

TEST_CASE_METHOD( MockPlayerYouTestsFixture,
                  "Give mock armour", "[single-file]" ) {

    item_def armour = simple_create_item(OBJ_ARMOUR, ARM_SCALE_MAIL);

    move_item_to_inv(armour);

    REQUIRE(you.base_ac_from(armour, 100) == 600);

    REQUIRE(any_items_of_type(OSEL_ANY));

    int num_armours = 0;

    for (int i=0; i < ENDOFPACK; i++)
    {
        item_def item = you.inv[i];
        if (item.base_type == OBJ_ARMOUR
               && item.sub_type == ARM_SCALE_MAIL)
        {
            num_armours++;
        }
    }

    REQUIRE(num_armours == 1);

    REQUIRE(you.base_ac(100) == 0);
}

static void make_and_equip_item(object_class_type base_type, int sub_type,
                                int plus = 0,
                                special_armour_type ego = SPARM_NORMAL);

static void find_and_equip_exact_item(item_def item){
    move_item_to_inv(item);

    int index =
        find_inv_index_with_exact_item(item.base_type, item.sub_type);

    REQUIRE(index != -1);

    equip_item(get_armour_slot(item), index);
}

static void make_and_equip_item(object_class_type base_type, int sub_type,
                                int plus, special_armour_type ego){

    item_def item = simple_create_item(base_type, sub_type, plus,
                                       ego);
    find_and_equip_exact_item(item);

}

TEST_CASE_METHOD( MockPlayerYouTestsFixture,
                  "Give mock armour and check after after putting it on",
                  "[single-file]" ) {

    make_and_equip_item(OBJ_ARMOUR, ARM_SCALE_MAIL);

    REQUIRE(you.base_ac(100) == 600);
}

TEST_CASE_METHOD( MockPlayerYouTestsFixture,
                  "Give mock robe and check after after putting it on",
                  "[single-file]" ) {

    make_and_equip_item(OBJ_ARMOUR, ARM_ROBE);

    REQUIRE(you.base_ac(100) == 200);
}

TEST_CASE_METHOD( MockPlayerYouTestsFixture,
                  "Give multiple mock items and check after putting it on",
                  "[single-file]" ) {

    make_and_equip_item(OBJ_ARMOUR, ARM_FIRE_DRAGON_ARMOUR);
    make_and_equip_item(OBJ_ARMOUR, ARM_HELMET);
    make_and_equip_item(OBJ_ARMOUR, ARM_BOOTS);
    make_and_equip_item(OBJ_ARMOUR, ARM_GLOVES);
    make_and_equip_item(OBJ_ARMOUR, ARM_CLOAK);

    REQUIRE(you.base_ac(100) == 1200);
}

TEST_CASE("armour_prop_test", "[single-file]"){
    REQUIRE(armour_prop(ARM_SCALE_MAIL, PARM_AC) == 6);
}

TEST_CASE("Test all_item_subtypes() does not include removed items",
          "[single-file]") {
    const auto items = all_item_subtypes(OBJ_POTIONS);

    const auto has_removed_item = find(items.begin(), items.end(), POT_POISON) != items.end();

    REQUIRE(has_removed_item == false);
}

TEST_CASE("Test all_item_subtypes() does include items for each category",
          "[single-file]") {
    // Note: CORPSES and RODS do not have any sub-items, so are excluded.
    REQUIRE(all_item_subtypes(OBJ_WEAPONS).size() > 0);
    REQUIRE(all_item_subtypes(OBJ_MISSILES).size() > 0);
    REQUIRE(all_item_subtypes(OBJ_ARMOUR).size() > 0);
    REQUIRE(all_item_subtypes(OBJ_WANDS).size() > 0);
    REQUIRE(all_item_subtypes(OBJ_SCROLLS).size() > 0);
    REQUIRE(all_item_subtypes(OBJ_JEWELLERY).size() > 0);
    REQUIRE(all_item_subtypes(OBJ_POTIONS).size() > 0);
    REQUIRE(all_item_subtypes(OBJ_BOOKS).size() > 0);
    REQUIRE(all_item_subtypes(OBJ_STAVES).size() > 0);
    REQUIRE(all_item_subtypes(OBJ_ORBS).size() > 0);
    REQUIRE(all_item_subtypes(OBJ_MISCELLANY).size() > 0);
    REQUIRE(all_item_subtypes(OBJ_GOLD).size() > 0);
    REQUIRE(all_item_subtypes(OBJ_RUNES).size() > 0);
}
