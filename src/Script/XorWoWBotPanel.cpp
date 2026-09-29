/*
 * XorWoW: the server side of the XorWoW client addon's altbot panels (BotPanel.lua, BotBags.lua,
 * BotQuests.lua).
 *
 * The addon whispers itself "XorWoW\tBOT;<verb>;<args>" (an addon whisper, swallowed here) and gets
 * its answers the same way, one record per addon message; a record is ';'-separated and names,
 * titles and item names have their ';' turned into ','. The bots' own chat replies are text for
 * people and change with every command, so the panels read the bots' state from here instead.
 *
 *   hello                          the panels are shown: no "=== Inventory ===" list in trades,
 *                                  answered with the roster
 *   roster                         BRS / BR;<name>;<class>;<level>;<race>;<online>;<own account> ... / BRE:
 *                                  the account's characters, then the player's other bots (addclass,
 *                                  guild characters, linked accounts)
 *   add;<name> / remove;<name>     .playerbots bot add / remove, its messages as system messages
 *   state;<bot>                    BSS;<bot>;<class>;<level>;<specs>;<active spec>;<points spec 1>;
 *                                  <points spec 2>;<dead>;<ghost>;<follow|stay|free>;<loot strategy>;
 *                                  <looting on>, then the strategies in chunks BSC;<a,b,...> (combat)
 *                                  and BSN (non-combat), then BSE;<bot>
 *   bags;<bot>                     BBS;<bot>;<money>;<conjured only>, BB;<bag>;<slot>;<item string>;
 *                                  <count>;<flags> per item (1 tradable, 2 soulbound, 4 in the trade,
 *                                  8 quest item, 16 conjured, 32 the bot can wear it, 64 a vendor buys
 *                                  it), BE;<equipment slot>;<item string>;<flags> per worn item,
 *                                  BBE;<bot>
 *   quests;<bot>                   BQS;<bot>, BQ;<id>;<level>;<0 incomplete|1 complete|2 failed>;
 *                                  <title>, BO;<id>;<have>;<need>;<text> per objective,
 *                                  BW;<id>;<index>;<item id>;<count> per reward to choose from, BQE;<bot>
 *   strategies;<bot>               BAS;<bot>, every strategy name the bot's AI knows (its class's
 *                                  included) in chunks BA;<a,b,...>, BAE;<bot>
 *   cmd;<bot>;<text>               <text> as if whispered to the bot (follow, co +aoe, drop <link>,
 *                                  e/ue/s <item link>, ll all...)
 *   reward;<bot>;<quest>;<index>   the bot turns the completed quest in with that reward: at the
 *                                  player's target or a quest ender of it next to the bot
 *   trade;<bot>;<bag>;<slot>       the bot puts that item into the open trade with the player
 *   untrade;<bot>;<trade slot>     the bot takes it out again
 *   gold;<bot>;<copper>            the bot's side of the trade offers that much money
 *
 * Unasked: BQC;<bot>;<quest> when an altbot turns in a quest with a choice of rewards for a player
 * whose addon shows the panels - the bot leaves the choice to the quest panel instead of picking.
 *
 * Errors come back as BX;<text>. Everything but "bags" (and the trade verbs) is for the player's
 * own altbots only - the bots in their PlayerbotMgr. "bags" also answers for any bot the player is
 * trading with, and the trade verbs keep the bots' trade rules: BotTradeConjuredOnly still holds a
 * bot of another account to its conjured items and no money, and soulbound stays soulbound.
 */

#include "XorWoWBotPanel.h"

#include "AiObjectContext.h"
#include "Bag.h"
#include "Chat.h"
#include "ChatHelper.h"
#include "Creature.h"
#include "GameObject.h"
#include "LootObjectStack.h"
#include "DBCStores.h"
#include "DataMap.h"
#include "DatabaseEnv.h"
#include "Item.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotMgr.h"
#include "Playerbots.h"
#include "QuestDef.h"
#include "ScriptMgr.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "StringFormat.h"
#include "TradeData.h"
#include "WorldPacket.h"
#include "WorldSession.h"

#include <algorithm>
#include <set>
#include <string>
#include <vector>

namespace
{
    constexpr char ADDON_PREFIX[] = "XorWoW";
    constexpr char PANEL_KEY[] = "xorwow-bot-panel";
    constexpr size_t MAX_BODY = 240;   // an addon message is 255 bytes with the prefix

    struct PanelState : public DataMap::Base
    {
        bool active = false;
    };

    void Send(Player* player, std::string const& body)
    {
        WorldPacket data;
        ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player, player,
                                     Acore::StringFormat("{}\t{}", ADDON_PREFIX, body.substr(0, MAX_BODY)));
        player->SendDirectMessage(&data);
    }

    std::string Clean(std::string text)
    {
        std::replace(text.begin(), text.end(), ';', ',');
        std::replace(text.begin(), text.end(), '\n', ' ');
        return text;
    }

    void Error(Player* player, std::string const& text) { Send(player, "BX;" + Clean(text)); }

    std::vector<std::string> SplitArgs(std::string const& text, size_t max)
    {
        // the last field keeps its ';' (a command text)
        std::vector<std::string> out;
        size_t start = 0;
        while (out.size() + 1 < max)
        {
            size_t const end = text.find(';', start);
            if (end == std::string::npos)
                break;
            out.push_back(text.substr(start, end - start));
            start = end + 1;
        }
        out.push_back(text.substr(start));
        return out;
    }

    // The player's own altbot by that name, online.
    Player* OwnBot(Player* player, std::string const& name)
    {
        PlayerbotMgr* mgr = GET_PLAYERBOT_MGR(player);
        Player* bot = mgr ? ObjectAccessor::FindPlayerByName(name) : nullptr;
        if (!bot || !mgr->GetPlayerBot(bot->GetGUID()) || !GET_PLAYERBOT_AI(bot))
            return nullptr;
        return bot;
    }

    // A bot trading with the player (their altbot or any other bot).
    Player* TradingBot(Player* player, std::string const& name)
    {
        Player* bot = ObjectAccessor::FindPlayerByName(name);
        if (!bot || !GET_PLAYERBOT_AI(bot) || IsSelfBot(bot) || bot->GetTrader() != player)
            return nullptr;
        return bot;
    }

    // Chunks of a comma list, each short enough for one record.
    void SendList(Player* player, std::string const& head, std::vector<std::string> const& names)
    {
        std::string body;
        for (std::string const& name : names)
        {
            std::string const entry = Clean(name);
            if (!body.empty() && head.size() + body.size() + entry.size() + 1 > MAX_BODY)
            {
                Send(player, head + body);
                body.clear();
            }
            body += body.empty() ? entry : "," + entry;
        }
        if (!body.empty())
            Send(player, head + body);
    }

    void SendRoster(Player* player)
    {
        PlayerbotMgr* mgr = GET_PLAYERBOT_MGR(player);
        Send(player, "BRS");

        std::vector<ObjectGuid> listed;
        if (QueryResult result = CharacterDatabase.Query(
                "SELECT guid, name, class, level, race FROM characters WHERE account = {} ORDER BY name",
                player->GetSession()->GetAccountId()))
        {
            do
            {
                Field* fields = result->Fetch();
                ObjectGuid const guid = ObjectGuid::Create<HighGuid::Player>(fields[0].Get<uint32>());
                if (guid == player->GetGUID())
                    continue;

                Player* bot = mgr ? mgr->GetPlayerBot(guid) : nullptr;
                listed.push_back(guid);
                Send(player, Acore::StringFormat("BR;{};{};{};{};{};1", fields[1].Get<std::string>(), fields[2].Get<uint8>(),
                                                 bot ? bot->GetLevel() : fields[3].Get<uint8>(), fields[4].Get<uint8>(),
                                                 bot ? 1 : 0));
            } while (result->NextRow());
        }

        // altbots of other (linked) accounts are online bots of the player too
        if (mgr)
            for (auto it = mgr->GetPlayerBotsBegin(); it != mgr->GetPlayerBotsEnd(); ++it)
            {
                Player* bot = it->second;
                if (bot && std::find(listed.begin(), listed.end(), bot->GetGUID()) == listed.end())
                    Send(player, Acore::StringFormat("BR;{};{};{};{};1;0", bot->GetName(), bot->getClass(),
                                                     bot->GetLevel(), bot->getRace()));
            }

        Send(player, "BRE");
    }

    void BotCommand(Player* player, std::string const& command, std::string const& name)
    {
        PlayerbotMgr* mgr = GET_PLAYERBOT_MGR(player);
        if (!mgr)
        {
            Error(player, "You cannot control bots yet");
            return;
        }

        std::string args = command + " " + name;   // HandlePlayerbotCommand cuts it up in place
        ChatHandler handler(player->GetSession());
        for (std::string const& message : mgr->HandlePlayerbotCommand(args.data(), player))
            handler.SendSysMessage(message);
    }

    // Talent points per tree for one spec: "31-20-0".
    std::string SpecPoints(Player* bot, uint8 spec)
    {
        uint32 points[3] = {0, 0, 0};
        uint32 const* tabs = GetTalentTabPages(bot->getClass());
        for (auto const& [spellId, talent] : bot->GetTalentMap())
        {
            if (!talent || talent->State == PLAYERSPELL_REMOVED || !talent->IsInSpec(spec))
                continue;

            TalentSpellPos const* pos = GetTalentSpellPos(spellId);
            TalentEntry const* entry = pos ? sTalentStore.LookupEntry(pos->talent_id) : nullptr;
            if (!entry)
                continue;

            SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId);
            uint32 const rank = info ? std::max<uint32>(info->GetRank(), 1) : 1;
            for (uint8 i = 0; i < 3; ++i)
                if (entry->TalentTab == tabs[i])
                    points[i] += rank;
        }
        return Acore::StringFormat("{}-{}-{}", points[0], points[1], points[2]);
    }

    void SendState(Player* player, Player* bot)
    {
        PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
        char const* move = botAI->HasStrategy("stay", BOT_STATE_NON_COMBAT) ? "stay"
                         : botAI->HasStrategy("follow", BOT_STATE_NON_COMBAT) ? "follow" : "free";

        LootStrategy* loot = botAI->GetAiObjectContext()->GetValue<LootStrategy*>("loot strategy")->Get();

        Send(player, Acore::StringFormat("BSS;{};{};{};{};{};{};{};{};{};{};{};{}", bot->GetName(), bot->getClass(), bot->GetLevel(),
                                         bot->GetSpecsCount(), bot->GetActiveSpec() + 1, SpecPoints(bot, 0),
                                         bot->GetSpecsCount() > 1 ? SpecPoints(bot, 1) : "",
                                         bot->isDead() ? 1 : 0, bot->HasPlayerFlag(PLAYER_FLAGS_GHOST) ? 1 : 0, move,
                                         loot ? loot->GetName() : "normal",
                                         botAI->HasStrategy("loot", BOT_STATE_NON_COMBAT) ? 1 : 0));
        SendList(player, "BSC;", botAI->GetStrategies(BOT_STATE_COMBAT));
        SendList(player, "BSN;", botAI->GetStrategies(BOT_STATE_NON_COMBAT));
        Send(player, "BSE;" + bot->GetName());
    }

    uint32 ItemFlags(Player* bot, Item* item, bool conjuredOnly)
    {
        ItemTemplate const* proto = item->GetTemplate();
        bool const conjured = proto->IsConjuredConsumable();
        uint32 flags = 0;
        if (item->CanBeTraded(false, true) && (!conjuredOnly || conjured))
            flags |= 1;
        if (item->IsSoulBound())
            flags |= 2;
        if (item->IsInTrade())
            flags |= 4;
        if (proto->Class == ITEM_CLASS_QUEST || proto->StartQuest)
            flags |= 8;
        if (conjured)
            flags |= 16;
        if (proto->InventoryType != INVTYPE_NON_EQUIP && bot->CanUseItem(proto) == EQUIP_ERR_OK)
            flags |= 32;
        if (proto->SellPrice > 0)
            flags |= 64;
        return flags;
    }

    std::string ItemString(Player* bot, Item* item)
    {
        return Acore::StringFormat("item:{}:{}:{}:{}:{}:0:{}:{}:{}", item->GetEntry(),
            item->GetEnchantmentId(PERM_ENCHANTMENT_SLOT), item->GetEnchantmentId(SOCK_ENCHANTMENT_SLOT),
            item->GetEnchantmentId(SOCK_ENCHANTMENT_SLOT_2), item->GetEnchantmentId(SOCK_ENCHANTMENT_SLOT_3),
            item->GetItemRandomPropertyId(), item->GetItemSuffixFactor(), bot->GetLevel());
    }

    void SendItem(Player* player, Player* bot, Item* item, uint8 bag, uint8 slot, bool conjuredOnly)
    {
        Send(player, Acore::StringFormat("BB;{};{};{};{};{}", bag, slot, ItemString(bot, item), item->GetCount(),
                                         ItemFlags(bot, item, conjuredOnly)));
    }

    void SendBags(Player* player, Player* bot)
    {
        bool const conjuredOnly = ConjuredOnlyFor(bot, player);
        Send(player, Acore::StringFormat("BBS;{};{};{}", bot->GetName(), bot->GetMoney(), conjuredOnly ? 1 : 0));

        for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
            if (Item* item = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
                SendItem(player, bot, item, INVENTORY_SLOT_BAG_0, slot, conjuredOnly);

        for (uint8 bagSlot = INVENTORY_SLOT_BAG_START; bagSlot < INVENTORY_SLOT_BAG_END; ++bagSlot)
            if (Bag* bag = bot->GetBagByPos(bagSlot))
                for (uint32 slot = 0; slot < bag->GetBagSize(); ++slot)
                    if (Item* item = bag->GetItemByPos(slot))
                        SendItem(player, bot, item, bagSlot, slot, conjuredOnly);

        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
            if (Item* item = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
                Send(player, Acore::StringFormat("BE;{};{};{}", slot, ItemString(bot, item), ItemFlags(bot, item, conjuredOnly)));

        Send(player, "BBE;" + bot->GetName());
    }

    std::string ObjectiveName(Quest const* quest, uint8 i)
    {
        if (!quest->ObjectiveText[i].empty())
            return quest->ObjectiveText[i];

        int32 const entry = quest->RequiredNpcOrGo[i];
        if (entry > 0)
        {
            if (CreatureTemplate const* creature = sObjectMgr->GetCreatureTemplate(entry))
                return creature->Name + " slain";
        }
        else if (GameObjectTemplate const* go = sObjectMgr->GetGameObjectTemplate(-entry))
            return go->name;
        return "?";
    }

    void SendQuests(Player* player, Player* bot)
    {
        Send(player, "BQS;" + bot->GetName());
        QuestStatusMap& statuses = bot->getQuestStatusMap();

        for (uint8 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
        {
            uint32 const questId = bot->GetQuestSlotQuestId(slot);
            Quest const* quest = questId ? sObjectMgr->GetQuestTemplate(questId) : nullptr;
            if (!quest)
                continue;

            QuestStatus const status = bot->GetQuestStatus(questId);
            Send(player, Acore::StringFormat("BQ;{};{};{};{}", questId, quest->GetQuestLevel(),
                                             status == QUEST_STATUS_COMPLETE ? 1 : status == QUEST_STATUS_FAILED ? 2 : 0,
                                             Clean(quest->GetTitle())));

            for (uint8 i = 0; i < QUEST_OBJECTIVES_COUNT; ++i)
                if (quest->RequiredNpcOrGo[i] && quest->RequiredNpcOrGoCount[i])
                    Send(player, Acore::StringFormat("BO;{};{};{};{}", questId, bot->GetQuestSlotCounter(slot, i),
                                                     quest->RequiredNpcOrGoCount[i], Clean(ObjectiveName(quest, i))));

            auto const data = statuses.find(questId);
            for (uint8 i = 0; i < QUEST_ITEM_OBJECTIVES_COUNT; ++i)
            {
                if (!quest->RequiredItemId[i] || !quest->RequiredItemCount[i])
                    continue;
                ItemTemplate const* proto = sObjectMgr->GetItemTemplate(quest->RequiredItemId[i]);
                uint32 const have = data != statuses.end() ? data->second.ItemCount[i] : 0;
                Send(player, Acore::StringFormat("BO;{};{};{};{}", questId, std::min<uint32>(have, quest->RequiredItemCount[i]),
                                                 quest->RequiredItemCount[i], Clean(proto ? proto->Name1 : "?")));
            }

            for (uint8 i = 0; i < quest->GetRewChoiceItemsCount(); ++i)
                if (quest->RewardChoiceItemId[i])
                    Send(player, Acore::StringFormat("BW;{};{};{};{}", questId, i, quest->RewardChoiceItemId[i],
                                                     quest->RewardChoiceItemCount[i]));
        }

        Send(player, "BQE;" + bot->GetName());
    }

    // Where the bot can turn the quest in: the player's target, else a quest ender next to the bot.
    Object* QuestEnder(Player* player, Player* bot, uint32 questId)
    {
        AiObjectContext* context = GET_PLAYERBOT_AI(bot)->GetAiObjectContext();
        auto const close = [bot](WorldObject* object) { return bot->IsWithinDistInMap(object, INTERACTION_DISTANCE + 5.0f); };

        if (Creature* target = ObjectAccessor::GetCreature(*player, player->GetTarget()))
            if (target->hasInvolvedQuest(questId) && close(target))
                return target;

        for (ObjectGuid const& guid : context->GetValue<GuidVector>("nearest npcs")->Get())
            if (Creature* npc = ObjectAccessor::GetCreature(*bot, guid))
                if (npc->hasInvolvedQuest(questId) && close(npc))
                    return npc;

        for (ObjectGuid const& guid : context->GetValue<GuidVector>("nearest game objects")->Get())
            if (GameObject* go = ObjectAccessor::GetGameObject(*bot, guid))
                if (go->hasInvolvedQuest(questId) && close(go))
                    return go;

        return nullptr;
    }

    void Reward(Player* player, Player* bot, uint32 questId, uint32 index)
    {
        Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
        if (!quest || bot->GetQuestStatus(questId) != QUEST_STATUS_COMPLETE || bot->GetQuestRewardStatus(questId))
            return Error(player, bot->GetName() + " has not completed that quest");
        if (index >= quest->GetRewChoiceItemsCount() || !quest->RewardChoiceItemId[index])
            return Error(player, "That quest has no such reward");

        Object* ender = QuestEnder(player, bot, questId);
        if (!ender)
            return Error(player, bot->GetName() + " must stand next to where the quest is turned in - target it and bring the bot close");
        if (!bot->CanRewardQuest(quest, index, false))
            return Error(player, bot->GetName() + " cannot take that reward (full bags, or a unique item it already has)");

        bot->RewardQuest(quest, index, ender, true);
        if (ItemTemplate const* proto = sObjectMgr->GetItemTemplate(quest->RewardChoiceItemId[index]))
            GET_PLAYERBOT_AI(bot)->TellMaster("Rewarded " + ChatHelper::FormatItem(proto));
    }

    void TradeItem(Player* player, Player* bot, uint8 bag, uint8 slot)
    {
        TradeData* trade = bot->GetTradeData();
        Item* item = bot->GetItemByPos(bag, slot);
        if (!trade || !item)
            return;

        if (item->IsInTrade())
            return;
        if (ConjuredOnlyFor(bot, player) && !item->GetTemplate()->IsConjuredConsumable())
        {
            Error(player, bot->GetName() + " can only give you conjured items");
            return;
        }
        if (!item->CanBeTraded(false, true))
        {
            Error(player, "That item cannot be traded");
            return;
        }

        int8 tradeSlot = -1;
        for (uint8 i = 0; i < TRADE_SLOT_TRADED_COUNT && tradeSlot < 0; ++i)
            if (!trade->GetItem(TradeSlots(i)))
                tradeSlot = i;
        if (tradeSlot < 0)
        {
            Error(player, "The trade window is full");
            return;
        }

        WorldPacket packet(CMSG_SET_TRADE_ITEM, 3);
        packet << uint8(tradeSlot) << bag << slot;
        bot->GetSession()->HandleSetTradeItemOpcode(packet);
    }

    void UntradeItem(Player* bot, uint8 tradeSlot)
    {
        if (!bot->GetTradeData() || tradeSlot >= TRADE_SLOT_TRADED_COUNT)
            return;

        WorldPacket packet(CMSG_CLEAR_TRADE_ITEM, 1);
        packet << tradeSlot;
        bot->GetSession()->HandleClearTradeItemOpcode(packet);
    }

    void TradeGold(Player* player, Player* bot, uint32 copper)
    {
        if (!bot->GetTradeData())
            return;
        if (copper && ConjuredOnlyFor(bot, player))
        {
            Error(player, bot->GetName() + " can only give you conjured items, not money");
            return;
        }

        WorldPacket packet(CMSG_SET_TRADE_GOLD, 4);
        packet << std::min<uint32>(copper, bot->GetMoney());
        bot->GetSession()->HandleSetTradeGoldOpcode(packet);
    }

    uint32 Number(std::string const& text)
    {
        return uint32(std::strtoul(text.c_str(), nullptr, 10));
    }

    void Handle(Player* player, std::string const& request)
    {
        std::vector<std::string> const head = SplitArgs(request, 2);
        std::string const& verb = head[0];
        std::string const rest = head.size() > 1 ? head[1] : "";

        if (verb == "hello")
        {
            player->CustomData.GetDefault<PanelState>(PANEL_KEY)->active = true;
            SendRoster(player);
            return;
        }
        if (verb == "roster")
            return SendRoster(player);
        if (verb == "add" || verb == "remove")
        {
            if (rest.empty() || rest.find(' ') != std::string::npos || rest.find(',') != std::string::npos)
                return Error(player, "One character name, please");
            return BotCommand(player, verb, rest);
        }

        if (verb == "bags" || verb == "trade" || verb == "untrade" || verb == "gold")
        {
            std::vector<std::string> const args = SplitArgs(rest, 3);
            Player* bot = TradingBot(player, args[0]);
            if (!bot && verb == "bags")
                bot = OwnBot(player, args[0]);
            if (!bot)
                return Error(player, verb == "bags" ? args[0] + " is not your altbot" : args[0] + " is not trading with you");

            if (verb == "bags")
                return SendBags(player, bot);
            if (verb == "trade" && args.size() == 3)
                TradeItem(player, bot, uint8(Number(args[1])), uint8(Number(args[2])));
            else if (verb == "untrade" && args.size() >= 2)
                UntradeItem(bot, uint8(Number(args[1])));
            else if (verb == "gold" && args.size() >= 2)
                TradeGold(player, bot, Number(args[1]));
            return SendBags(player, bot);
        }

        std::vector<std::string> const args = SplitArgs(rest, 2);
        Player* bot = OwnBot(player, args[0]);
        if (!bot)
            return Error(player, args[0] + " is not your altbot, or is not online");

        if (verb == "state")
            return SendState(player, bot);
        if (verb == "quests")
            return SendQuests(player, bot);
        if (verb == "strategies")
        {
            std::set<std::string> const all = GET_PLAYERBOT_AI(bot)->GetAiObjectContext()->GetSupportedStrategies();
            Send(player, "BAS;" + bot->GetName());
            SendList(player, "BA;", std::vector<std::string>(all.begin(), all.end()));
            Send(player, "BAE;" + bot->GetName());
            return;
        }
        if (verb == "reward" && args.size() == 2)
        {
            std::vector<std::string> const quest = SplitArgs(args[1], 2);
            if (quest.size() == 2)
                Reward(player, bot, Number(quest[0]), Number(quest[1]));
            return SendQuests(player, bot);
        }
        if (verb == "cmd" && args.size() == 2 && !args[1].empty())
        {
            // the bots' usual path: the same checks as a whisper, and their reply is a whisper
            GET_PLAYERBOT_AI(bot)->HandleCommand(CHAT_MSG_WHISPER, args[1], player);
            return;
        }
        Error(player, "Unknown request " + verb);
    }
}

bool XorWoWBotPanelActive(Player* player)
{
    PanelState* state = player ? player->CustomData.Get<PanelState>(PANEL_KEY) : nullptr;
    return state && state->active;
}

void XorWoWBotPanelRewardPending(Player* master, Player* bot, uint32 questId)
{
    if (XorWoWBotPanelActive(master) && bot)
        Send(master, Acore::StringFormat("BQC;{};{}", bot->GetName(), questId));
}

class XorWoWBotPanelPlayerScript : public PlayerScript
{
public:
    XorWoWBotPanelPlayerScript() : PlayerScript("XorWoWBotPanelPlayerScript", {PLAYERHOOK_CAN_PLAYER_USE_PRIVATE_CHAT}) {}

    using PlayerScript::OnPlayerCanUseChat;

    bool OnPlayerCanUseChat(Player* player, uint32 type, uint32 lang, std::string& msg, Player* receiver) override
    {
        if (lang != LANG_ADDON || type != CHAT_MSG_WHISPER || receiver != player)
            return true;

        std::string const prefix = Acore::StringFormat("{}\tBOT;", ADDON_PREFIX);
        if (msg.rfind(prefix, 0) != 0)
            return true;

        if (!player->GetSession()->IsBot())
            Handle(player, msg.substr(prefix.size()));
        return false;
    }
};

void AddXorWoWBotPanelScripts()
{
    new XorWoWBotPanelPlayerScript();
}
