/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "TradeAction.h"
#include "ChatHelper.h"
#include "Event.h"
#include "ItemCountValue.h"
#include "ItemVisitors.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"

#include <algorithm>

// BotTradeConjuredOnly: a real player who is not on the bot's own account gets conjured items only
static bool ConjuredOnlyFor(Player* bot, Player* player)
{
    return sPlayerbotAIConfig.botTradeConjuredOnly && player && player->GetSession() &&
           (IsRealPlayer(player) || IsSelfBot(player)) &&
           bot->GetSession()->GetAccountId() != player->GetSession()->GetAccountId();
}

// The spell that makes what was asked for (a normalized conjured request), if this bot has one
static std::string ConjureSpellFor(Player* bot, std::string const& request)
{
    switch (bot->getClass())
    {
        case CLASS_MAGE:
            if (request == "conjured food")
                return "conjure food";
            if (request == "conjured water")
                return "conjure water";
            break;
        case CLASS_WARLOCK:
            if (request == "healthstone")
                return "create healthstone";
            break;
        default:
            break;
    }
    return "";
}

bool TradeAction::Execute(Event event)
{
    std::string text = event.getParam();

    // "water" / "food" / "hs" from a group member: the conjured kind only, and by its keyword
    std::string const conjured = sPlayerbotAIConfig.conjuredItemsForGroup
                                     ? PlayerbotAI::NormalizeConjuredRequest(text) : "";
    if (!conjured.empty())
        text = conjured;

    // If text starts with any excluded prefix, don't process it further.
    for (auto const& prefix : sPlayerbotAIConfig.tradeActionExcludedPrefixes)
    {
        if (text.find(prefix) == 0)
            return false;
    }

    if (!bot->GetTrader())
    {
        GuidVector guids = chat->parseGameobjects(text);
        Player* player = nullptr;

        for (auto& guid : guids)
            if (guid.IsPlayer())
                player = ObjectAccessor::FindPlayer(guid);

        // a group member asking for conjured items gets them, not the master
        if (!player && botAI->CanRequestConjuredItems(event.getOwner()) && botAI->IsConjuredItemRequest(text))
            player = event.getOwner();

        if (!player && botAI->GetMaster())
            player = botAI->GetMaster();

        if (!player)
            return false;

        if (!player->GetTrader())
        {
            // a conjured request fills the window as soon as the player opens it (TradeStatusAction)
            if (sPlayerbotAIConfig.conjuredItemsForGroup && botAI->IsConjuredItemRequest(text))
            {
                pendingRequest = text;
                pendingFor = player->GetGUID();
            }

            WorldPacket packet(CMSG_INITIATE_TRADE);
            packet << player->GetGUID();
            bot->GetSession()->HandleInitiateTradeOpcode(packet);
            return true;
        }
        else if (player->GetTrader() != bot)
            return false;
    }

    uint32 copper = chat->parseMoney(text);
    if (copper > 0)
    {
        WorldPacket packet(CMSG_SET_TRADE_GOLD, 4);
        packet << copper;
        bot->GetSession()->HandleSetTradeGoldOpcode(packet);
    }

    size_t pos = text.rfind(" ");
    int count = pos != std::string::npos ? atoi(text.substr(pos + 1).c_str()) : 1;
    if (!conjured.empty())
        count = 1;   // one stack

    std::vector<Item*> found = parseItems(text);

    Player* trader = bot->GetTrader();
    if (trader && ConjuredOnlyFor(bot, trader))
    {
        found.erase(std::remove_if(found.begin(), found.end(),
                                   [](Item* item) { return !item->GetTemplate()->IsConjuredConsumable(); }),
                    found.end());
    }

    if (!conjured.empty())
    {
        // a whole stack at once: the fullest one first
        std::stable_sort(found.begin(), found.end(),
                         [](Item* a, Item* b) { return a->GetCount() > b->GetCount(); });
    }

    if (found.empty() && trader && !conjured.empty())
    {
        // nothing to give yet: conjure it now, the player asks again in a moment
        std::string const spell = ConjureSpellFor(bot, conjured);
        if (!spell.empty() && botAI->HasSpell(spell))
        {
            bot->Whisper("Give me a moment to conjure some, then ask again", LANG_UNIVERSAL, trader);
            botAI->DoSpecificAction(spell, Event(), true);
            return true;
        }
    }

    if (found.empty())
        return false;

    uint32 traded = 0;
    for (Item* item : found)
    {
        if (!bot->GetTrader() || item->IsInTrade())
            continue;

        int8 slot = item->CanBeTraded() ? -1 : TRADE_SLOT_NONTRADED;
        if (TradeItem(item, slot) && slot != TRADE_SLOT_NONTRADED && ++traded >= uint32(count))
            break;
    }

    return true;
}

bool TradeAction::FillPending(Player* trader)
{
    if (pendingRequest.empty() || !trader || trader->GetGUID() != pendingFor)
        return false;

    std::string const text = pendingRequest;
    pendingRequest.clear();
    pendingFor.Clear();

    return Execute(Event("trade", text, trader));
}

bool TradeAction::TradeItem(Item const* item, int8 slot)
{
    int8 tradeSlot = -1;
    Item* itemPtr = const_cast<Item*>(item);

    TradeData* pTrade = bot->GetTradeData();
    if ((slot >= 0 && slot < TRADE_SLOT_COUNT) && pTrade->GetItem(TradeSlots(slot)) == nullptr)
        tradeSlot = slot;

    if (slot == TRADE_SLOT_NONTRADED)
        pTrade->SetItem(TRADE_SLOT_NONTRADED, itemPtr);
    else
    {
        for (uint8 i = 0; i < TRADE_SLOT_TRADED_COUNT && tradeSlot == -1; i++)
        {
            if (pTrade->GetItem(TradeSlots(i)) == itemPtr)
            {
                tradeSlot = i;

                WorldPacket packet(CMSG_CLEAR_TRADE_ITEM, 1);
                packet << (uint8)tradeSlot;
                bot->GetSession()->HandleClearTradeItemOpcode(packet);
                pTrade->SetItem(TradeSlots(i), nullptr);
                return true;
            }
        }

        for (uint8 i = 0; i < TRADE_SLOT_TRADED_COUNT && tradeSlot == -1; i++)
        {
            if (pTrade->GetItem(TradeSlots(i)) == nullptr)
                tradeSlot = i;
        }
    }

    if (tradeSlot == -1)
        return false;

    WorldPacket packet(CMSG_SET_TRADE_ITEM, 3);
    packet << (uint8)tradeSlot;
    packet << (uint8)item->GetBagSlot();
    packet << (uint8)item->GetSlot();
    bot->GetSession()->HandleSetTradeItemOpcode(packet);
    return true;
}
