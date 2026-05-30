/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#include "RangedCombatStrategy.h"

#include "Playerbots.h"

void RangedCombatStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    CombatStrategy::InitTriggers(triggers);

    triggers.push_back(new TriggerNode("enemy too close for spell",
                                        { NextAction("flee", ACTION_MOVE + 4) }));

    // When out of mana and an enemy is in melee range, don't just stand there
    // being hit: try a wand/ranged shot (no mana), otherwise melee attack.
    // Mana regenerates meanwhile; once it's back up the higher-priority spell
    // triggers take over and the bot resumes casting automatically.
    // Low relevance (1.0) so any real spell (5.0+) always wins when castable.
    triggers.push_back(new TriggerNode("low mana",
                                        { NextAction("shoot", 1.1f), NextAction("melee", 1.0f) }));
}
