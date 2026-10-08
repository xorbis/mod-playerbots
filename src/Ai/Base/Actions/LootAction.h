/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_LOOTACTION_H
#define PLAYERBOTS_LOOTACTION_H

#include "InventoryAction.h"
#include "MovementActions.h"

class GameObject;
class LootObject;
class PlayerbotAI;
class SpellInfo;

class LootAction : public MovementAction
{
public:
    LootAction(PlayerbotAI* botAI) : MovementAction(botAI, "loot") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

class OpenLootAction : public MovementAction
{
public:
    OpenLootAction(PlayerbotAI* botAI) : MovementAction(botAI, "open loot") {}

    bool Execute(Event event) override;

private:
    bool DoLoot(LootObject& lootObject);
    uint32 GetOpeningSpell(LootObject& lootObject);
    uint32 GetOpeningSpell(LootObject& lootObject, GameObject* go);
    bool CanOpenLock(LootObject& lootObject, SpellInfo const* spellInfo, GameObject* go);
    bool CanOpenLock(uint32 skillId, uint32 reqSkillValue);
};

// XorWoW: TCG cards (items 261001-261999, xorwow_tcg.cpp in the core) stay with the players: bots
// never loot them and always pass on them, wherever they drop.
inline bool IsXorWoWTcgCard(uint32 itemId)
{
    return itemId > 261000 && itemId < 262000;
}

class StoreLootAction : public InventoryAction
{
public:
    StoreLootAction(PlayerbotAI* botAI) : InventoryAction(botAI, "store loot") {}

    bool Execute(Event event) override;
    static bool IsLootAllowed(uint32 itemid, PlayerbotAI* botAI);
};

class ReleaseLootAction : public InventoryAction
{
public:
    ReleaseLootAction(PlayerbotAI* botAI) : InventoryAction(botAI, "release loot") {}

    bool Execute(Event event) override;
};

#endif
