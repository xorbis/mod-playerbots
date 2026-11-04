/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_AQ40ACTIONCONTEXT_H
#define PLAYERBOTS_AQ40ACTIONCONTEXT_H

#include "Action.h"
#include "Aq40Actions.h"
#include "NamedObjectContext.h"

class RaidAq40ActionContext : public NamedObjectContext<Action>
{
public:
    RaidAq40ActionContext()
    {
        creators["aq40 cthun avoid dark glare"] = &RaidAq40ActionContext::cthun_avoid_dark_glare;
        creators["aq40 cthun stomach exit"] = &RaidAq40ActionContext::cthun_stomach_exit;
    }

private:
    static Action* cthun_avoid_dark_glare(PlayerbotAI* ai) { return new Aq40CthunAvoidDarkGlareAction(ai); }
    static Action* cthun_stomach_exit(PlayerbotAI* ai) { return new Aq40CthunStomachExitAction(ai); }
};

#endif
