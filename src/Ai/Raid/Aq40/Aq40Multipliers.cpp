/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "Aq40Multipliers.h"
#include "Aq40Actions.h"
#include "Aq40Utils.h"
#include "AttackAction.h"
#include "Playerbots.h"

float Aq40CthunDarkGlareMovementMultiplier::GetValue(Action* action)
{
    // Attacking does not move the bot, only movement actions matter
    if (!dynamic_cast<MovementAction*>(action) || dynamic_cast<AttackAction*>(action) ||
        dynamic_cast<Aq40CthunAvoidDarkGlareAction*>(action))
        return 1.0f;

    if (bot->GetPositionZ() < 0.0f)
        return 1.0f;

    Creature* eye = RaidAq40Utils::GetEyeOfCthun(bot);
    CthunGlareSweep sweep;
    return RaidAq40Utils::GetGlareSweep(eye, sweep) ? 0.0f : 1.0f;
}
