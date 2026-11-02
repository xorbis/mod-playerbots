/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "Aq40Triggers.h"
#include "Aq40Utils.h"

bool Aq40CthunDarkGlareTrigger::IsActive()
{
    // Bots in the stomach (below the floor) are out of the beam's reach
    if (!bot->IsAlive() || bot->GetPositionZ() < 0.0f)
        return false;

    Creature* eye = RaidAq40Utils::GetEyeOfCthun(bot);
    CthunGlareSweep sweep;
    if (!RaidAq40Utils::GetGlareSweep(eye, sweep))
        return false;

    float escapeAngle;
    return RaidAq40Utils::GetGlareEscapeAngle(bot, eye, sweep, escapeAngle);
}
