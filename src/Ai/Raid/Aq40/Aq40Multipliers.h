/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_AQ40MULTIPLIERS_H
#define PLAYERBOTS_AQ40MULTIPLIERS_H

#include "Multiplier.h"

// XorWoW: during Dark Glare the dodge is the only way a bot moves. Otherwise bots that got clear walk
// straight back into the beam's path to reach their Eye Tentacle.
class Aq40CthunDarkGlareMovementMultiplier : public Multiplier
{
public:
    Aq40CthunDarkGlareMovementMultiplier(PlayerbotAI* ai) : Multiplier(ai, "aq40 cthun dark glare movement") {}
    float GetValue(Action* action) override;
};

#endif
