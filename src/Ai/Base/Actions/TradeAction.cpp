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
#include "ConjuredItems.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"

#include <algorithm>

static uint32 const CONJURED_REQUEST_MAX_CASTS = 12;    // 2 per cast at low ranks: enough for a stack
static uint32 const CONJURED_REQUEST_TIMEOUT = 90;      // seconds before a request is dropped
static float const CONJURED_REQUEST_TRADE_RANGE = 9.0f; // the trade distance, a bit under it

void TradeAction::SetPending(std::string const& request, Player* player)
{
    if (pendingRequest != request || pendingFor != player->GetGUID())
    {
        pendingCasts = 0;
        pendingTold = false;
        pendingSince = time(nullptr);
    }
    pendingRequest = request;
    pendingFor = player->GetGUID();
}

void TradeAction::ClearPending()
{
    pendingRequest.clear();
    pendingFor.Clear();
    pendingSince = 0;
    pendingCasts = 0;
    pendingTold = false;
}

uint32 TradeAction::PendingTarget() const { return pendingRequest == "healthstone" ? 1 : 20; }

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
            if (!conjured.empty())
            {
                // remembered until served: conjured first when short (ContinuePending), put in the
                // window as soon as the player opens it (FillPending)
                SetPending(conjured, player);
                if (UsableConjuredCount(botAI, conjured, player) < PendingTarget() &&
                    ConjureSpellIdFor(bot, conjured, player->GetLevel()))
                    return ContinuePending();
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

    if (trader && !conjured.empty())
    {
        // only what the requester can use, a whole stack at once: the fullest one first
        found.erase(std::remove_if(found.begin(), found.end(),
                                   [trader](Item* item) { return trader->CanUseItem(item->GetTemplate()) != EQUIP_ERR_OK; }),
                    found.end());
        std::stable_sort(found.begin(), found.end(),
                         [](Item* a, Item* b) { return a->GetCount() > b->GetCount(); });

        uint32 have = 0;
        for (Item* item : found)
            have += item->GetCount();

        // not a full stack yet: conjure first, the pending machinery calls back here - but only
        // when a conjure can really start and we are not already inside ContinuePending(). In
        // combat it cannot cast, so pendingCasts never grows and it would hand straight back to
        // us: the two would call each other until the stack ran out (a whispered "water" in a
        // fight crashed the worldserver).
        if (!servingPending && !bot->IsInCombat() && have < PendingTarget() &&
            pendingCasts < CONJURED_REQUEST_MAX_CASTS &&
            ConjureSpellIdFor(bot, conjured, trader->GetLevel()))
        {
            SetPending(conjured, trader);
            return ContinuePending();
        }

        if (found.empty())
        {
            bot->Whisper("I have none you could use and cannot conjure any right now", LANG_UNIVERSAL, trader);
            ClearPending();
            return false;
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

    if (!conjured.empty() && traded)
        ClearPending();

    return true;
}

bool TradeAction::FillPending(Player* trader)
{
    if (pendingRequest.empty() || !trader || trader->GetGUID() != pendingFor)
        return false;

    return Execute(Event("trade", pendingRequest, trader));
}

bool TradeAction::ContinuePending()
{
    if (pendingRequest.empty())
        return false;

    Player* player = ObjectAccessor::FindPlayer(pendingFor);
    if (!player || !player->IsInWorld() || !bot->IsInWorld() ||
        (!botAI->CanRequestConjuredItems(player) && player != botAI->GetMaster()) ||
        time(nullptr) - pendingSince > CONJURED_REQUEST_TIMEOUT)
    {
        ClearPending();
        return false;
    }

    if (bot->IsNonMeleeSpellCast(false))
        return false;   // a conjure is under way

    uint32 const spellId = ConjureSpellIdFor(bot, pendingRequest, player->GetLevel());
    if (UsableConjuredCount(botAI, pendingRequest, player) < PendingTarget() && spellId &&
        pendingCasts < CONJURED_REQUEST_MAX_CASTS && !bot->IsInCombat())
    {
        if (!pendingTold)
        {
            bot->Whisper("Give me a moment, I am conjuring some for you", LANG_UNIVERSAL, player);
            pendingTold = true;
        }

        ++pendingCasts;
        if (botAI->CastSpell(spellId, bot))
            return true;
        // the cast did not start (mana, bags): hand over what there is
    }

    if (!UsableConjuredCount(botAI, pendingRequest, player))
    {
        bot->Whisper("I cannot conjure any you could use right now", LANG_UNIVERSAL, player);
        ClearPending();
        return false;
    }

    // enough, or as much as it gets: trade
    if (Player* trader = bot->GetTrader())
    {
        if (trader != player)
            return false;   // busy with someone else: keep waiting

        servingPending = true;   // Execute() must not send the request back here
        bool const served = Execute(Event("trade", pendingRequest, player));
        servingPending = false;
        return served;
    }

    if (player->GetTrader())
        return false;   // the player is trading with someone else

    if (!bot->IsWithinDistInMap(player, CONJURED_REQUEST_TRADE_RANGE))
        return false;   // out of trade range: wait until they are close

    WorldPacket packet(CMSG_INITIATE_TRADE);
    packet << player->GetGUID();
    bot->GetSession()->HandleInitiateTradeOpcode(packet);
    return true;   // TRADE_STATUS_OPEN_WINDOW -> FillPending
}

bool ContinueConjuredRequestAction::Execute(Event /*event*/)
{
    TradeAction* trade = dynamic_cast<TradeAction*>(context->GetAction("trade"));
    return trade && trade->ContinuePending();
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
