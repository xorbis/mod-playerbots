/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "Aq40Actions.h"
#include "Aq40Utils.h"

bool Aq40CthunAvoidDarkGlareAction::Execute(Event /*event*/)
{
    Creature* eye = RaidAq40Utils::GetEyeOfCthun(bot);
    CthunGlareSweep sweep;
    if (!RaidAq40Utils::GetGlareSweep(eye, sweep))
        return false;

    float escapeAngle;
    if (!RaidAq40Utils::GetGlareEscapeAngle(bot, eye, sweep, escapeAngle))
        return false;

    // Keep the same distance from the eye, so the bot circles it instead of cutting across
    float radius = std::max(bot->GetExactDist2d(eye), 8.0f);
    float x = eye->GetPositionX() + radius * std::cos(escapeAngle);
    float y = eye->GetPositionY() + radius * std::sin(escapeAngle);
    float z = bot->GetPositionZ();
    bot->UpdateAllowedPositionZ(x, y, z);

    if (bot->IsNonMeleeSpellCast(false))
        bot->InterruptNonMeleeSpells(false);

    return MoveTo(bot->GetMapId(), x, y, z, false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
}
