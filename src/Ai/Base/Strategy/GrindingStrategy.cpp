/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#include "GrindingStrategy.h"

#include "Playerbots.h"

std::vector<NextAction> GrindingStrategy::getDefaultActions()
{
    return {
        NextAction("drink", 4.2f),
        NextAction("food", 4.1f),
    };
}

void GrindingStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    // No target: attack any nearby mob; if there's nothing to attack, wander
    // around (move random) to look for new mobs instead of standing still.
    // attack anything (4.0) wins when a mob is in range; move random (1.5) only
    // runs as the lower-priority fallback when no mob is found.
    // reduce lower than loot
    triggers.push_back(
        new TriggerNode(
            "no target",
            {
                NextAction("attack anything", 4.0f),
                NextAction("move random", 1.5f)
            }
        )
    );
}

void MoveRandomStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(
        new TriggerNode(
            "often",
            {
                NextAction("move random", 1.5f)
            }
        )
    );
}
