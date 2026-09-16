/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_TRADEACTION_H
#define PLAYERBOTS_TRADEACTION_H

#include "InventoryAction.h"

class Item;
class PlayerbotAI;

class TradeAction : public InventoryAction
{
public:
    TradeAction(PlayerbotAI* botAI) : InventoryAction(botAI, "trade") {}

    bool Execute(Event event) override;

    // ConjuredItemsForGroup: a request ("conjured water", "conjured food", "healthstone") that is
    // still being conjured, or waiting for its trade window
    bool HasPendingRequest() const { return !pendingRequest.empty(); }
    bool FillPending(Player* trader);   // the window opened: put the items in
    bool ContinuePending();             // polled: conjure until there is enough, then trade

private:
    bool TradeItem(Item const* item, int8 slot);
    void SetPending(std::string const& request, Player* player);
    void ClearPending();
    uint32 PendingTarget() const;       // a stack of food/water, one healthstone

    std::string pendingRequest;
    ObjectGuid pendingFor;
    time_t pendingSince = 0;
    uint8 pendingCasts = 0;
    bool pendingTold = false;

    static std::map<std::string, uint32> slots;
};

// "continue conjured request": what the polling trigger runs
class ContinueConjuredRequestAction : public Action
{
public:
    ContinueConjuredRequestAction(PlayerbotAI* botAI) : Action(botAI, "continue conjured request") {}

    bool Execute(Event event) override;
    bool isUsefulWhenStunned() override { return false; }
};

#endif
