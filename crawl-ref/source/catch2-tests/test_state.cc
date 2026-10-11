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
