/*
 * XorWoW: the altbot panels of the XorWoW client addon talk to this (XorWoWBotPanel.cpp).
 */

#ifndef _PLAYERBOT_XORWOW_BOT_PANEL_H
#define _PLAYERBOT_XORWOW_BOT_PANEL_H

#include "Define.h"

class Player;

// The player's addon shows its altbot panels: the bots leave out what those panels show
// (the "=== Inventory ===" list when a trade opens).
bool XorWoWBotPanelActive(Player* player);

// An altbot turns in a quest with a choice of rewards for a master whose addon shows the panels:
// the choice is left to the quest panel (the "reward" request), told to the addon here.
void XorWoWBotPanelRewardPending(Player* master, Player* bot, uint32 questId);

#endif
