/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_CONJUREDITEMS_H
#define PLAYERBOTS_CONJUREDITEMS_H

#include "Action.h"

class Player;
class PlayerbotAI;

// ConjuredItemsForGroup helpers. A "request" is the normalized keyword from
// PlayerbotAI::NormalizeConjuredRequest: "conjured food", "conjured water" or "healthstone".

// Whether this bot's class is the one that makes `request` at all: a mage for food and water,
// a warlock for healthstones. A request said in party chat reaches every bot in the group, so
// the ones that cannot make it must leave it alone rather than answer.
bool CanServeConjuredRequest(Player* bot, std::string const& request);

// Highest rank of the spell that makes `request` this bot knows whose item a level `forLevel`
// player can use; 0 when there is none. Mage food/water (Conjure Refreshment from 74 on),
// warlock healthstone (the rank's item follows the core's spell_warl_create_healthstone table).
uint32 ConjureSpellIdFor(Player* bot, std::string const& request, uint8 forLevel);

// Required level of the item ConjureSpellIdFor's spell makes, 0 when there is none: the yardstick
// for "as good as this bot can make for that player".
uint8 BestConjuredItemLevelFor(Player* bot, std::string const& request, uint8 forLevel);

// How many of `request` the bot carries that `forPlayer` can use and that are no worse than the
// rank it could conjure for them - every rank it carries with bestRankOnly = false, or when
// forPlayer is null.
uint32 UsableConjuredCount(PlayerbotAI* botAI, std::string const& request, Player* forPlayer,
                           bool bestRankOnly = true);

// Destroy the `request` stacks weaker than `keepFromLevel`, once the bot holds some of that rank
// and no group member is too low to use it.
void DropObsoleteConjured(PlayerbotAI* botAI, std::string const& request, uint8 keepFromLevel);

// The lowest level among the real players in the bot's group, 0 without one.
uint8 LowestRealPlayerLevelInGroup(PlayerbotAI* botAI);

// Conjure a rank the lowest-level real player in the group can use (the stock triggers)
class ConjureForGroupAction : public Action
{
public:
    ConjureForGroupAction(PlayerbotAI* botAI, std::string const name, std::string const request)
        : Action(botAI, name), request(request) {}

    bool Execute(Event event) override;
    bool isUseful() override;

private:
    std::string const request;
};

class ConjureWaterForGroupAction : public ConjureForGroupAction
{
public:
    ConjureWaterForGroupAction(PlayerbotAI* botAI) : ConjureForGroupAction(botAI, "conjure water for group", "conjured water") {}
};

class ConjureFoodForGroupAction : public ConjureForGroupAction
{
public:
    ConjureFoodForGroupAction(PlayerbotAI* botAI) : ConjureForGroupAction(botAI, "conjure food for group", "conjured food") {}
};

#endif
