#include "catch.hpp"

#include "AppHdr.h"

#include "state.h"

TEST_CASE("Command replay queries use their own game state", "[state]")
{
    game_state session;
    REQUIRE_FALSE(session.is_replaying_keys());

    session.doing_prev_cmd_again = true;
    REQUIRE(session.is_replaying_keys());

    session.doing_prev_cmd_again = false;
    session.repeat_cmd = CMD_WAIT;
    REQUIRE(session.is_replaying_keys());
}

TEST_CASE("Game type qualifiers use their own game state", "[state]")
{
    game_state session;
    REQUIRE(session.game_type_qualifier().empty());

    session.type = GAME_TYPE_SPRINT;
    REQUIRE(session.game_type_qualifier() == "-sprint");

    session.type = GAME_TYPE_TUTORIAL;
    REQUIRE(session.game_type_qualifier() == "-tutorial");

    session.type = GAME_TYPE_HINTS;
    REQUIRE(session.game_type_qualifier() == "-hints");

    session.type = GAME_TYPE_CUSTOM_SEED;
    REQUIRE(session.game_type_qualifier() == "-seeded");
}

TEST_CASE("Wizard repeat policy checks the supplied player", "[state]")
{
    game_state session;
    player actor;
    session.repeat_cmd = CMD_WIZARD;
    actor.duration[DUR_PARALYSIS] = 1;

    REQUIRE_FALSE(session.can_cancel_cmd_repeat(actor));

    actor.duration[DUR_PARALYSIS] = 0;
    actor.turn_is_over = true;
    REQUIRE_FALSE(session.can_cancel_cmd_repeat(actor));

    actor.turn_is_over = false;
    REQUIRE(session.can_cancel_cmd_repeat(actor));

    session.repeat_cmd = CMD_NO_CMD;
    REQUIRE_FALSE(session.can_cancel_cmd_repeat(actor));
    REQUIRE(session.can_cancel_cmd_repeat(actor, true));
}

TEST_CASE("Rest turn accounting belongs to the game state", "[state]")
{
    game_state first;
    game_state second;
    REQUIRE_FALSE(first.place_stat_counts_as_rest(false));
    REQUIRE(first.place_stat_counts_as_rest(true));
    REQUIRE(first.place_stat_counts_as_rest(false));
    REQUIRE_FALSE(first.place_stat_counts_as_rest(false));
    REQUIRE_FALSE(second.place_stat_counts_as_rest(false));

    first.place_stat_counts_as_rest(true);
    first.reset_game();
    REQUIRE_FALSE(first.place_stat_counts_as_rest(false));
}

TEST_CASE("Pickup and inscription requests belong to one game", "[state]")
{
    game_state first;
    game_state second;
    REQUIRE_FALSE(first.needs_autopickup());
    REQUIRE_FALSE(first.needs_autoinscribe());

    first.request_autopickup();
    first.request_autoinscribe();
    REQUIRE(first.needs_autopickup());
    REQUIRE(first.needs_autoinscribe());
    REQUIRE_FALSE(second.needs_autopickup());
    REQUIRE_FALSE(second.needs_autoinscribe());

    first.request_autopickup(false);
    REQUIRE_FALSE(first.needs_autopickup());
    first.reset_game();
    REQUIRE_FALSE(first.needs_autoinscribe());
}

TEST_CASE("Checkpoint policy uses the supplied turn and game state", "[state]")
{
    game_state session;
    REQUIRE_FALSE(session.should_checkpoint_after_turn(256, false));

    session.save_after_turn = true;
    REQUIRE(session.should_checkpoint_after_turn(1, false));
    REQUIRE_FALSE(session.should_checkpoint_after_turn(1, true));

    session.disables.set(DIS_SAVE_CHECKPOINTS);
    REQUIRE_FALSE(session.should_checkpoint_after_turn(1, false));
    session.disables.set(DIS_SAVE_CHECKPOINTS, false);

    session.save_after_turn = false;
    session.type = GAME_TYPE_SPRINT;
    REQUIRE_FALSE(session.should_checkpoint_after_turn(255, false));
    REQUIRE(session.should_checkpoint_after_turn(256, false));
    REQUIRE_FALSE(session.should_checkpoint_after_turn(256, true));
}
