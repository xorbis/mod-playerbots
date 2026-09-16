/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "SummoningRitualAction.h"

#include "Event.h"
#include "GameObject.h"
#include "LastMovementValue.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Playerbots.h"
#include "PositionValue.h"
#include "ServerFacade.h"

#include <unordered_set>

// The spell a participant channels for the ritual; the core drops whoever stops channeling it
static uint32 RitualChannelSpell(GameObject* ritual)
{
    uint32 animSpell = ritual->GetGOInfo()->summoningRitual.animSpell;
    return animSpell ? animSpell : ritual->GetSpellId();
}

// A ritual that summons a player (meeting stone) summons whoever its owner targets
static bool SummonsOwnerTarget(GameObject* ritual)
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(ritual->GetGOInfo()->summoningRitual.spellId);
    return spellInfo && spellInfo->HasEffect(SPELL_EFFECT_SUMMON_PLAYER);
}

bool JoinSummoningRitualAction::IsParticipant(Player* player, GameObject* ritual)
{
    Spell* channel = player->GetCurrentSpell(CURRENT_CHANNELED_SPELL);
    return channel && channel->m_spellInfo->Id == RitualChannelSpell(ritual);
}

bool JoinSummoningRitualAction::IsRitualChannel(Player* player)
{
    Spell* channel = player->GetCurrentSpell(CURRENT_CHANNELED_SPELL);
    if (!channel)
        return false;

    // the portal's own spell, cast with its effects ignored: a participant has no portal of it
    SpellInfo const* spellInfo = channel->m_spellInfo;
    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
    {
        if (spellInfo->Effects[i].Effect != SPELL_EFFECT_TRANS_DOOR)
            continue;

        GameObjectTemplate const* goInfo = sObjectMgr->GetGameObjectTemplate(spellInfo->Effects[i].MiscValue);
        if (goInfo && goInfo->type == GAMEOBJECT_TYPE_SUMMONING_RITUAL)
            return !player->GetGameObject(spellInfo->Id);
    }

    // the anim spell of a portal that has one
    static std::unordered_set<uint32> const animSpells = []
    {
        std::unordered_set<uint32> spells;
        for (auto const& itr : *sObjectMgr->GetGameObjectTemplates())
        {
            GameObjectTemplate const& goInfo = itr.second;
            if (goInfo.type == GAMEOBJECT_TYPE_SUMMONING_RITUAL && goInfo.summoningRitual.animSpell)
                spells.insert(goInfo.summoningRitual.animSpell);
        }
        return spells;
    }();

    return animSpells.count(spellInfo->Id) != 0;
}

GameObject* JoinSummoningRitualAction::FindRitual(Player* bot)
{
    Group* group = bot->GetGroup();
    if (!group)
        return nullptr;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* summoner = ref->GetSource();
        if (!summoner || summoner == bot || summoner->FindMap() != bot->FindMap())
            continue;

        // the portal exists only while its owner channels the spell that spawned it
        Spell* channel = summoner->GetCurrentSpell(CURRENT_CHANNELED_SPELL);
        if (!channel)
            continue;

        GameObject* ritual = summoner->GetGameObject(channel->m_spellInfo->Id);
        if (!ritual || !ritual->IsInWorld() || ritual->GetGoType() != GAMEOBJECT_TYPE_SUMMONING_RITUAL)
            continue;

        // the one being summoned cannot help summon itself
        if (summoner->GetTarget() == bot->GetGUID() && SummonsOwnerTarget(ritual))
            continue;

        if (bot->GetDistance(ritual) > sPlayerbotAIConfig.sightDistance)
            continue;

        return ritual;
    }

    return nullptr;
}

bool JoinSummoningRitualAction::isUseful()
{
    GameObject* ritual = FindRitual(bot);
    if (!ritual)
        return IsRitualChannel(bot);

    if (IsParticipant(bot, ritual))
        return true;

    // everyone the ritual needs is already channeling
    if (ritual->GetUniqueUseCount() >= ritual->GetGOInfo()->summoningRitual.reqParticipants)
        return false;

    // told to stay: help only when the portal is within reach
    if (botAI->HasStrategy("stay", BOT_STATE_NON_COMBAT) && bot->GetDistance(ritual) > INTERACTION_DISTANCE)
        return false;

    return true;
}

bool JoinSummoningRitualAction::Execute(Event /*event*/)
{
    GameObject* ritual = FindRitual(bot);
    if (!ritual)
    {
        // a ritual that ran out leaves its participants channeling; the core only ends the
        // channel when the ritual completes or the portal is removed
        if (!IsRitualChannel(bot))
            return false;

        bot->InterruptSpell(CURRENT_CHANNELED_SPELL);
        return true;
    }

    // taking part: succeed without doing anything, so no other action moves the bot or casts
    if (IsParticipant(bot, ritual))
        return true;

    if (bot->GetDistance(ritual) > INTERACTION_DISTANCE)
        return MoveNear(ritual, 2.0f);

    // stand still and face the portal like a player about to click it
    AI_VALUE(LastMovement&, "last movement").clear();
    bot->GetMotionMaster()->Clear(false);
    bot->StopMoving();
    if (bot->IsSitState())
        bot->SetStandState(UNIT_STAND_STATE_STAND);
    ServerFacade::instance().SetFacingTo(bot, ritual);

    ritual->Use(bot);
    return IsParticipant(bot, ritual);
}

bool AcceptSummonAction::Execute(Event event)
{
    WorldPacket p(event.getPacket());
    p.rpos(0);
    ObjectGuid summonerGuid;
    p >> summonerGuid;

    Player* summoner = ObjectAccessor::FindPlayer(summonerGuid);
    if (!summoner || !bot->IsInSameRaidWith(summoner))
        return false;

    WorldPacket packet(CMSG_SUMMON_RESPONSE, 8 + 1);
    packet << summonerGuid;
    packet << uint8(1);  // accept
    bot->GetSession()->HandleSummonResponseOpcode(packet);

    // declined by the core (dead, in combat, the request expired)
    if (!bot->IsBeingTeleported())
        return false;

    bot->GetMotionMaster()->Clear();
    AI_VALUE(LastMovement&, "last movement").clear();

    // a bot told to stay would otherwise walk back to where it was
    if (botAI->HasStrategy("stay", BOT_STATE_NON_COMBAT))
    {
        PositionMap& posMap = AI_VALUE(PositionMap&, "position");
        PositionInfo stayPosition = posMap["stay"];
        stayPosition.Set(summoner->GetPositionX(), summoner->GetPositionY(), summoner->GetPositionZ(),
                         summoner->GetMapId());
        posMap["stay"] = stayPosition;
    }

    return true;
}
