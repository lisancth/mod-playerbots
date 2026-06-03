/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#include "MaintenanceStrategy.h"

#include "Playerbots.h"

std::vector<NextAction> MaintenanceStrategy::getDefaultActions()
{
    // 卖货后返回出发点（优先级 10，isUseful() 自己判断是否需要执行）
    return { NextAction("return from vendor", 10.0f) };
}

void MaintenanceStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(
        new TriggerNode(
            "seldom",
            {
                NextAction("clean quest log", 6.0f)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "random",
            {
                NextAction("use random recipe", 1.0f)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "random",
            {
                NextAction("disenchant random item", 1.0f)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "random",
            {
                NextAction("enchant random item", 1.0f)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "random",
            {
                NextAction("smart destroy item", 1.0f)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "move stuck",
            {
                NextAction("reset", 1.0f)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "random",
            {
                NextAction("use random quest item", 0.9f)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "random",
            {
                NextAction("auto share quest", 0.9f)
            }
        )
    );
    // 背包满时：先寻路到最近商人(优先级9)，到达后卖货(优先级8)
    triggers.push_back(
        new TriggerNode(
            "bag full sell",
            {
                NextAction("travel to vendor", 9.0f),
                NextAction("bag full sell", 8.0f)
            }
        )
    );
}
