/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_AQ40TRIGGERS_H
#define PLAYERBOTS_AQ40TRIGGERS_H

#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "Trigger.h"

class Aq40CthunDarkGlareTrigger : public Trigger
{
public:
    Aq40CthunDarkGlareTrigger(PlayerbotAI* botAI) : Trigger(botAI, "aq40 cthun dark glare") {}
    bool IsActive() override;
};

// XorWoW: both Flesh Tentacles are dead, time to leave the stomach.
class Aq40CthunStomachExitTrigger : public Trigger
{
public:
    Aq40CthunStomachExitTrigger(PlayerbotAI* botAI) : Trigger(botAI, "aq40 cthun stomach exit") {}
    bool IsActive() override;
};
#endif
