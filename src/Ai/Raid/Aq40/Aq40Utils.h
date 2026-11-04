/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_AQ40UTILS_H
#define PLAYERBOTS_AQ40UTILS_H

#include "Creature.h"
#include "Player.h"

// XorWoW: Eye of C'Thun's Dark Glare. The eye faces the last Green Beam target, turns red, then
// sweeps a 5-degree beam through half a circle (pi/35 per second, 35 ticks) in a random direction.
struct CthunGlareSweep
{
    float startAngle = 0.0f;
    int8 direction = 0;  // +1 orientation grows, -1 shrinks, 0 not moving yet
    float beamAngle = 0.0f;
};

// XorWoW: the stomach exit, areatrigger 4033 on the island between the two Flesh Tentacles.
float const CTHUN_STOMACH_EXIT_X = -8546.23f;
float const CTHUN_STOMACH_EXIT_Y = 1987.65f;
float const CTHUN_STOMACH_EXIT_Z = -96.52f;

class RaidAq40Utils
{
public:
    static Creature* GetEyeOfCthun(Player* bot);
    static bool IsInCthunStomach(Player* bot);
    // Fills the sweep state while the eye is casting Dark Glare, returns false otherwise.
    static bool GetGlareSweep(Creature* eye, CthunGlareSweep& sweep);
    // Absolute angle (around the eye) the bot should run to, or false when it is out of the beam's path.
    static bool GetGlareEscapeAngle(Player* bot, Creature* eye, CthunGlareSweep const& sweep, float& escapeAngle);
};

#endif
