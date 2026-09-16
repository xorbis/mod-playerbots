/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_SUMMONINGRITUALTRIGGERS_H
#define PLAYERBOTS_SUMMONINGRITUALTRIGGERS_H

#include "Trigger.h"

class PlayerbotAI;

// A group member is channeling a summoning ritual the bot could take part in, or the bot is still
// channeling for a ritual that is over. Checked every tick (cheap: the group's members and their
// current channel) so that a bot taking part keeps its "join summoning ritual" action ahead of
// anything that would move it.
class SummoningRitualTrigger : public Trigger
{
public:
    SummoningRitualTrigger(PlayerbotAI* botAI) : Trigger(botAI, "summoning ritual") {}

    bool IsActive() override;
};

#endif
