/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "Aq40Actions.h"
#include "Aq40Utils.h"
#include "ObjectMgr.h"
#include "ScriptMgr.h"

uint32 const AT_CTHUN_STOMACH_EXIT = 4033;
uint32 const AT_CTHUN_CENTER = 4036;
float const AT_CTHUN_CENTER_RADIUS = 7.0f;
uint32 const STOMACH_EXIT_RETRY_MS = 10000;

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

bool Aq40CthunStomachExitAction::Execute(Event /*event*/)
{
    if (bot->IsNonMeleeSpellCast(false))
        bot->InterruptNonMeleeSpells(false);

    // Inside the 5-yard trigger, with some margin
    if (bot->GetExactDist2d(CTHUN_STOMACH_EXIT_X, CTHUN_STOMACH_EXIT_Y) > 4.0f)
    {
        MoveTo(bot->GetMapId(), CTHUN_STOMACH_EXIT_X, CTHUN_STOMACH_EXIT_Y, CTHUN_STOMACH_EXIT_Z, false, false, false,
               false, MovementPriority::MOVEMENT_COMBAT);
        return true;
    }

    // The script takes 4 seconds to throw the bot out and it must stay near the exit meanwhile
    if (lastExitMSTime && getMSTimeDiff(lastExitMSTime, getMSTime()) < STOMACH_EXIT_RETRY_MS)
        return true;

    AreaTrigger const* exit = sObjectMgr->GetAreaTrigger(AT_CTHUN_STOMACH_EXIT);
    if (!exit)
        return false;

    lastExitMSTime = getMSTime();
    bot->StopMoving();
    sScriptMgr->OnAreaTrigger(bot, exit);

    // Thrown out on top of C'Thun: a client reports the center trigger on landing, which spits the player out
    Player* player = bot;
    player->m_Events.AddEventAtOffset([player]()
    {
        AreaTrigger const* center = sObjectMgr->GetAreaTrigger(AT_CTHUN_CENTER);
        if (center && player->IsAlive() && player->GetMapId() == center->map &&
            player->GetExactDist2d(center->x, center->y) <= AT_CTHUN_CENTER_RADIUS)
            sScriptMgr->OnAreaTrigger(player, center);
    }, 6s);

    return true;
}
