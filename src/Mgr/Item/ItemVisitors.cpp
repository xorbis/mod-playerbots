/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ItemVisitors.h"
#include "Playerbots.h"

bool FindUsableItemVisitor::Visit(Item* item)
{
    if (bot->CanUseItem(item->GetTemplate()) == EQUIP_ERR_OK)
        return FindItemVisitor::Visit(item);

    return true;
}

bool FindPotionVisitor::Accept(ItemTemplate const* proto)
{
    if (proto->Class == ITEM_CLASS_CONSUMABLE &&
        (proto->SubClass == ITEM_SUBCLASS_POTION || proto->SubClass == ITEM_SUBCLASS_FLASK))
    {
        for (uint8 j = 0; j < MAX_ITEM_PROTO_SPELLS; j++)
        {
            SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(proto->Spells[j].SpellId);
            if (!spellInfo)
                return false;

            for (uint8 i = 0; i < 3; i++)
            {
                if (spellInfo->Effects[i].Effect == effectId)
                    return true;
            }
        }
    }

    return false;
}

// A mana regen aura on the spell, or on one it triggers (Refreshment: a food spell and a drink spell)
static bool SpellRestoresMana(uint32 spellId, uint8 depth = 0)
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return false;

    for (SpellEffectInfo const& effect : spellInfo->Effects)
    {
        if (effect.Effect == SPELL_EFFECT_TRIGGER_SPELL)
        {
            if (depth < 2 && SpellRestoresMana(effect.TriggerSpell, depth + 1))
                return true;
        }
        else if (effect.Effect == SPELL_EFFECT_APPLY_AURA && effect.MiscValue == POWER_MANA &&
                 (effect.ApplyAuraName == SPELL_AURA_MOD_POWER_REGEN || effect.ApplyAuraName == SPELL_AURA_OBS_MOD_POWER ||
                  effect.ApplyAuraName == SPELL_AURA_PERIODIC_ENERGIZE))
            return true;
    }

    return false;
}

bool FindFoodVisitor::Accept(ItemTemplate const* proto)
{
    return proto->Class == ITEM_CLASS_CONSUMABLE &&
           (proto->SubClass == ITEM_SUBCLASS_CONSUMABLE || proto->SubClass == ITEM_SUBCLASS_FOOD) &&
           proto->Spells[0].SpellCategory == spellCategory && (!conjured || proto->IsConjuredConsumable()) &&
           (!drink || SpellRestoresMana(proto->Spells[0].SpellId));
}

bool FindMountVisitor::Accept(ItemTemplate const* proto)
{
    for (uint8 j = 0; j < MAX_ITEM_PROTO_SPELLS; j++)
    {
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(proto->Spells[j].SpellId);
        if (!spellInfo)
            return false;

        for (uint8 i = 0; i < 3; i++)
        {
            if (spellInfo->Effects[i].ApplyAuraName == SPELL_AURA_MOUNTED)
                return true;
        }
    }

    return false;
}

bool FindPetVisitor::Accept(ItemTemplate const* proto)
{
    if (proto->Class == ITEM_CLASS_MISC)
    {
        for (uint8 j = 0; j < MAX_ITEM_PROTO_SPELLS; j++)
        {
            SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(proto->Spells[j].SpellId);
            if (!spellInfo)
                return false;

            for (uint8 i = 0; i < 3; i++)
            {
                if (spellInfo->Effects[i].Effect == SPELL_EFFECT_SUMMON_PET)
                    return true;
            }
        }
    }

    return false;
}

FindItemUsageVisitor::FindItemUsageVisitor(Player* bot, ItemUsage usage) : FindUsableItemVisitor(bot), usage(usage)
{
    context = GET_PLAYERBOT_AI(bot)->GetAiObjectContext();
};

bool FindItemUsageVisitor::Accept(ItemTemplate const* proto)
{
    if (AI_VALUE2(ItemUsage, "item usage", proto->ItemId) == usage)
        return true;

    return false;
}

bool FindUsableNamedItemVisitor::Accept(ItemTemplate const* proto)
{
    return proto && !proto->Name1.empty() && strstri(proto->Name1.c_str(), name.c_str());
}
