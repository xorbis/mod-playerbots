/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "Aq40Utils.h"

#include <algorithm>
#include <mutex>
#include <unordered_map>

#include "GameTime.h"

uint32 const NPC_EYE_OF_CTHUN = 15589;
uint32 const SPELL_CTHUN_RED_COLORATION = 22518;  // on the eye for the whole Dark Glare

// Beam is 5 degrees wide; stay well clear of it and of the angle where the sweep ends.
float const GLARE_SAFETY_ANGLE = 12.0f * M_PI / 180.0f;
// How far ahead along the circle a bot runs per move order.
float const GLARE_MAX_STEP_ANGLE = 40.0f * M_PI / 180.0f;
float const GLARE_STEP_DISTANCE = 12.0f;

namespace
{
    struct GlareRecord
    {
        float startAngle;
        int8 direction;
        time_t seenAt;
    };

    std::mutex glareMutex;
    std::unordered_map<ObjectGuid, GlareRecord> glareRecords;

    // (-pi, pi]
    float NormalizeSigned(float angle)
    {
        angle = Position::NormalizeOrientation(angle);
        return angle > M_PI ? angle - 2.0f * M_PI : angle;
    }
}

Creature* RaidAq40Utils::GetEyeOfCthun(Player* bot)
{
    Creature* eye = bot->FindNearestCreature(NPC_EYE_OF_CTHUN, 150.0f);
    return eye && eye->IsAlive() && eye->IsInCombat() ? eye : nullptr;
}

bool RaidAq40Utils::IsInCthunStomach(Player* bot)
{
    // Other parts of the instance go below zero as well, the stomach is the area under C'Thun
    return bot->GetPositionZ() < -40.0f && bot->GetExactDist2d(CTHUN_STOMACH_EXIT_X, CTHUN_STOMACH_EXIT_Y) < 100.0f;
}

bool RaidAq40Utils::GetGlareSweep(Creature* eye, CthunGlareSweep& sweep)
{
    std::lock_guard<std::mutex> lock(glareMutex);

    if (!eye || !eye->HasAura(SPELL_CTHUN_RED_COLORATION))
    {
        if (eye)
            glareRecords.erase(eye->GetGUID());
        return false;
    }

    // The eye is turned to the start angle in the same update that applies the red coloration,
    // and holds it for 3 seconds before the first tick, so the first bot to look records it.
    time_t now = GameTime::GetGameTime().count();
    float orientation = eye->GetOrientation();
    auto itr = glareRecords.find(eye->GetGUID());
    if (itr == glareRecords.end() || now - itr->second.seenAt > 60)
        itr = glareRecords.insert_or_assign(eye->GetGUID(), GlareRecord{orientation, 0, now}).first;

    GlareRecord& record = itr->second;
    if (!record.direction)
    {
        float moved = NormalizeSigned(orientation - record.startAngle);
        if (std::fabs(moved) > 0.01f)
            record.direction = moved > 0.0f ? 1 : -1;
    }

    sweep.startAngle = record.startAngle;
    sweep.direction = record.direction;
    sweep.beamAngle = orientation;
    return true;
}

bool RaidAq40Utils::GetGlareEscapeAngle(Player* bot, Creature* eye, CthunGlareSweep const& sweep, float& escapeAngle)
{
    float botAngle = eye->GetAngle(bot);
    float fromStart = NormalizeSigned(botAngle - sweep.startAngle);

    if (!sweep.direction)
    {
        // Direction unknown: only the start angle is certain to be hit, step to whichever side is closer.
        if (std::fabs(fromStart) >= GLARE_SAFETY_ANGLE)
            return false;

        float side = fromStart != 0.0f ? (fromStart > 0.0f ? 1.0f : -1.0f) : (bot->GetGUID().GetCounter() % 2 ? 1.0f : -1.0f);
        escapeAngle = sweep.startAngle + side * GLARE_SAFETY_ANGLE * 1.5f;
        return true;
    }

    // Work along the sweep: 0 = start, pi = end. Negative values are the half the beam never visits.
    float dir = sweep.direction;
    float botPos = NormalizeSigned(dir * (botAngle - sweep.startAngle));
    float beamPos = std::clamp(NormalizeSigned(dir * (sweep.beamAngle - sweep.startAngle)), 0.0f, float(M_PI));

    // Behind the beam (it has already passed) or on the never-visited half, clear of the end angle.
    if (botPos < beamPos && botPos > -float(M_PI) + GLARE_SAFETY_ANGLE)
        return false;

    // Ahead of the beam: run the same way it turns, past the end of the sweep. Crossing back through
    // the beam would get the bot hit, so it always goes around, never across.
    float target = float(M_PI) + GLARE_SAFETY_ANGLE * 1.5f;
    float current = botPos < 0.0f ? botPos + 2.0f * float(M_PI) : botPos;
    float radius = std::max(bot->GetExactDist2d(eye), 8.0f);
    float step = std::min({target - current, GLARE_MAX_STEP_ANGLE, GLARE_STEP_DISTANCE / radius});
    if (step <= 0.0f)
        return false;

    escapeAngle = sweep.startAngle + dir * (current + step);
    return true;
}
