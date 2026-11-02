/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "MindControlledMemberValue.h"
#include "Group.h"
#include "Playerbots.h"
#include "Spell.h"
#include "SpellInfo.h"
#include "SpellMgr.h"

namespace
{
    // Best first: the long incapacitates, then the short stuns and fears. None of them deals damage, so the
    // charmed player is not hurt. Spells that only root or silence would leave them free to attack.
    std::vector<std::string> const& GetClassCcSpells(uint8 classId)
    {
        static std::vector<std::string> const none;
        static std::vector<std::string> const warrior = { "intimidating shout", "concussion blow" };
        static std::vector<std::string> const paladin = { "repentance", "hammer of justice" };
        static std::vector<std::string> const hunter = { "wyvern sting", "scatter shot" };
        static std::vector<std::string> const rogue = { "blind", "gouge" };
        static std::vector<std::string> const shaman = { "hex" };
        static std::vector<std::string> const mage = { "polymorph" };
        static std::vector<std::string> const warlock = { "fear" };
        static std::vector<std::string> const druid = { "cyclone" };

        switch (classId)
        {
            case CLASS_WARRIOR:
                return warrior;
            case CLASS_PALADIN:
                return paladin;
            case CLASS_HUNTER:
                return hunter;
            case CLASS_ROGUE:
                return rogue;
            case CLASS_SHAMAN:
                return shaman;
            case CLASS_MAGE:
                return mage;
            case CLASS_WARLOCK:
                return warlock;
            case CLASS_DRUID:
                return druid;
            default:
                return none;
        }
    }

    bool IsCrowdControlSpell(SpellInfo const* spellInfo)
    {
        return spellInfo->HasAura(SPELL_AURA_MOD_CONFUSE) || spellInfo->HasAura(SPELL_AURA_MOD_FEAR) ||
               spellInfo->HasAura(SPELL_AURA_MOD_STUN);
    }
}

Unit* MindControlledMemberValue::Calculate()
{
    Group* group = bot->GetGroup();
    if (!group)
        return nullptr;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == bot || !member->IsInMap(bot) || !member->IsAlive() || !member->IsCharmed())
            continue;

        // Charmed by an enemy, not a vehicle or a friendly possession
        if (!bot->IsValidAttackTarget(member) ||
            !bot->IsWithinDistInMap(member, sPlayerbotAIConfig.sightDistance))
            continue;

        if (IsCrowdControlled(member) || IsBeingCrowdControlled(member))
            continue;

        return member;
    }

    return nullptr;
}

bool MindControlledMemberValue::IsCrowdControlled(Unit* target)
{
    return target->HasAuraType(SPELL_AURA_MOD_CONFUSE) || target->HasAuraType(SPELL_AURA_MOD_FEAR) ||
           target->HasAuraType(SPELL_AURA_MOD_STUN);
}

// Another group member is already casting a crowd control spell on the target, one is enough
bool MindControlledMemberValue::IsBeingCrowdControlled(Unit* target)
{
    for (GroupReference* ref = bot->GetGroup()->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == bot || !member->IsInMap(bot))
            continue;

        Spell* spell = member->GetCurrentSpell(CURRENT_GENERIC_SPELL);
        if (spell && spell->m_targets.GetUnitTargetGUID() == target->GetGUID() &&
            IsCrowdControlSpell(spell->m_spellInfo))
            return true;
    }

    return false;
}

std::string MindControlledMemberValue::SelectCcSpell(PlayerbotAI* botAI, Unit* target)
{
    Player* bot = botAI->GetBot();
    for (std::string const& spell : GetClassCcSpells(bot->getClass()))
    {
        uint32 spellId = botAI->GetAiObjectContext()->GetValue<uint32>("spell id", spell)->Get();
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
        if (!spellInfo)
            continue;

        // CanCastSpell lets out of range through and the cast does not walk the bot over, so check it here
        bool inRange = spellInfo->RangeEntry && (spellInfo->RangeEntry->Flags & SPELL_RANGE_MELEE)
                           ? bot->IsWithinMeleeRange(target)
                           : bot->IsWithinDistInMap(target, spellInfo->GetMaxRange(false, bot));
        if (!inRange || !bot->IsWithinLOSInMap(target))
            continue;

        if (botAI->CanCastSpell(spellId, target))
            return spell;
    }

    return "";
}
