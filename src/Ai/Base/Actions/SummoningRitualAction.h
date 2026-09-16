/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_SUMMONINGRITUALACTION_H
#define PLAYERBOTS_SUMMONINGRITUALACTION_H

#include "MovementActions.h"

class GameObject;
class Player;
class PlayerbotAI;

// A group member channels a summoning ritual (meeting stone, warlock Ritual of Summoning or Souls,
// mage Ritual of Refreshment): walk to the portal, click it and keep channeling until the core ends
// the channel - the ritual completed, or the portal is gone.
class JoinSummoningRitualAction : public MovementAction
{
public:
    JoinSummoningRitualAction(PlayerbotAI* botAI) : MovementAction(botAI, "join summoning ritual") {}

    bool Execute(Event event) override;
    bool isUseful() override;

    // The portal a group member is channeling that the bot could take part in, or nullptr
    static GameObject* FindRitual(Player* bot);
    // Channeling the ritual's participant spell, which is how the core counts the participants
    static bool IsParticipant(Player* player, GameObject* ritual);
    // Channeling as the participant of some ritual whose portal it does not own
    static bool IsRitualChannel(Player* player);
};

// The bot is the one being summoned: accept, like a player answering the summon prompt
class AcceptSummonAction : public Action
{
public:
    AcceptSummonAction(PlayerbotAI* botAI) : Action(botAI, "accept summon") {}

    bool Execute(Event event) override;
};

#endif
