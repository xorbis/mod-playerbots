/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ConjuredItems.h"
#include "ItemCountValue.h"
#include "ObjectMgr.h"
#include "Playerbots.h"
#include "SpellInfo.h"
#include "SpellMgr.h"

#include <algorithm>

bool CanServeConjuredRequest(Player* bot, std::string const& request)
{
    switch (bot->getClass())
    {
        case CLASS_MAGE:
            return request == "conjured food" || request == "conjured water";
        case CLASS_WARLOCK:
            return request == "healthstone";
        default:
            return false;
    }
}

uint32 ConjureSpellIdFor(Player* bot, std::string const& request, uint8 forLevel)
{
    if (!CanServeConjuredRequest(bot, request))
        return 0;

    std::vector<std::string> names;
    if (bot->getClass() == CLASS_MAGE)
        names = { "conjure refreshment", request == "conjured food" ? "conjure food" : "conjure water" };
    else
        names = { "create healthstone" };

    // Create Healthstone rank -> item, first column of spell_warl_create_healthstone::iTypes in
    // the core (the Improved Healthstone variants share the required level)
    static uint32 const healthstoneByRank[8] = { 5512, 5511, 5509, 5510, 9421, 22103, 36889, 36892 };

    uint32 best = 0;
    uint32 bestLevel = 0;
    // lower ranks stay known (Active = false) once a higher one is learnt; a server-side cast
    // of them works, and that is what a low-level requester needs
    for (auto const& [spellId, playerSpell] : bot->GetSpellMap())
    {
        if (!bot->HasSpell(spellId))
            continue;

        SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId);
        if (!info)
            continue;

        std::string name = info->SpellName[0];
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);
        if (std::find(names.begin(), names.end(), name) == names.end())
            continue;

        uint32 itemId = 0;
        if (info->Effects[0].Effect == SPELL_EFFECT_CREATE_ITEM)
            itemId = info->Effects[0].ItemType;
        else if (name == "create healthstone")
        {
            uint8 const rank = info->GetRank();
            if (rank >= 1 && rank <= 8)
                itemId = healthstoneByRank[rank - 1];
        }

        ItemTemplate const* proto = itemId ? sObjectMgr->GetItemTemplate(itemId) : nullptr;
        if (!proto || proto->RequiredLevel > forLevel || bot->CanUseItem(proto) != EQUIP_ERR_OK)
            continue;

        if (!best || proto->RequiredLevel > bestLevel)
        {
            best = spellId;
            bestLevel = proto->RequiredLevel;
        }
    }

    return best;
}

uint32 UsableConjuredCount(PlayerbotAI* botAI, std::string const& request, Player* forPlayer)
{
    uint32 count = 0;
    for (Item* item : botAI->GetAiObjectContext()->GetValue<std::vector<Item*>>("inventory items", request)->Get())
        if (!forPlayer || forPlayer->CanUseItem(item->GetTemplate()) == EQUIP_ERR_OK)
            count += item->GetCount();

    return count;
}

uint8 LowestRealPlayerLevelInGroup(PlayerbotAI* botAI)
{
    uint8 level = 0;
    for (Player* player : botAI->GetRealPlayersInGroup())
        if (!level || player->GetLevel() < level)
            level = player->GetLevel();

    return level;
}

bool ConjureForGroupAction::isUseful()
{
    return sPlayerbotAIConfig.conjuredItemsForGroup && !bot->IsInCombat() && !bot->IsNonMeleeSpellCast(false);
}

bool ConjureForGroupAction::Execute(Event /*event*/)
{
    uint8 const level = LowestRealPlayerLevelInGroup(botAI);
    if (!level)
        return false;

    uint32 const spellId = ConjureSpellIdFor(bot, request, level);
    return spellId && botAI->CastSpell(spellId, bot);
}
