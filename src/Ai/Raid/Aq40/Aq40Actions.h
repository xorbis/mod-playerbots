/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_AQ40ACTIONS_H
#define PLAYERBOTS_AQ40ACTIONS_H

#include "MovementActions.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"

class Aq40CthunAvoidDarkGlareAction : public MovementAction
{
public:
    Aq40CthunAvoidDarkGlareAction(PlayerbotAI* botAI, std::string const name = "aq40 cthun avoid dark glare")
        : MovementAction(botAI, name)
    {
    }
    bool Execute(Event event) override;
};

// XorWoW: walk to the stomach exit once both Flesh Tentacles are dead. Areatriggers are reported by
// the game client, which bots do not have, so the action reports them itself.
class Aq40CthunStomachExitAction : public MovementAction
{
public:
    Aq40CthunStomachExitAction(PlayerbotAI* botAI, std::string const name = "aq40 cthun stomach exit")
        : MovementAction(botAI, name)
    {
    }
    bool Execute(Event event) override;

private:
    uint32 lastExitMSTime = 0;
};
#endif
