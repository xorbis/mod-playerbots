/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_MINDCONTROLLEDMEMBERVALUE_H
#define PLAYERBOTS_MINDCONTROLLEDMEMBERVALUE_H

#include "Value.h"

class PlayerbotAI;
class Unit;

// XorWoW: a group member charmed by an enemy (Dominate Mind, Mind Control...) that nobody has crowd controlled yet
class MindControlledMemberValue : public UnitCalculatedValue
{
public:
    MindControlledMemberValue(PlayerbotAI* botAI) : UnitCalculatedValue(botAI, "mind controlled member", 1000) {}

    Unit* Calculate() override;

    // Already polymorphed, feared, stunned or asleep
    static bool IsCrowdControlled(Unit* target);
    // The first crowd control spell of the bot's class that can land on the target from where the bot stands,
    // empty when there is none
    static std::string SelectCcSpell(PlayerbotAI* botAI, Unit* target);

private:
    bool IsBeingCrowdControlled(Unit* target);
};

#endif
