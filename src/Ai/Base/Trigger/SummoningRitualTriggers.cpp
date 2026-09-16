/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "SummoningRitualTriggers.h"

#include "Playerbots.h"
#include "SummoningRitualAction.h"

bool SummoningRitualTrigger::IsActive()
{
    return JoinSummoningRitualAction::FindRitual(bot) || JoinSummoningRitualAction::IsRitualChannel(bot);
}
