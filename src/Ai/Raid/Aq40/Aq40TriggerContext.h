/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_AQ40TRIGGERCONTEXT_H
#define PLAYERBOTS_AQ40TRIGGERCONTEXT_H

#include "Aq40Triggers.h"
#include "NamedObjectContext.h"

class RaidAq40TriggerContext : public NamedObjectContext<Trigger>
{
public:
    RaidAq40TriggerContext()
    {
        creators["aq40 cthun dark glare"] = &RaidAq40TriggerContext::cthun_dark_glare;
        creators["aq40 cthun stomach exit"] = &RaidAq40TriggerContext::cthun_stomach_exit;
    }

private:
    static Trigger* cthun_dark_glare(PlayerbotAI* ai) { return new Aq40CthunDarkGlareTrigger(ai); }
    static Trigger* cthun_stomach_exit(PlayerbotAI* ai) { return new Aq40CthunStomachExitTrigger(ai); }
};

#endif
