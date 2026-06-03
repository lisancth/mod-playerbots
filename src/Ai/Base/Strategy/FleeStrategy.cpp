/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#include "FleeStrategy.h"

#include "Playerbots.h"

void FleeStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    // panic 和 outnumbered 保留但不触发跑路，让 bot 正常战斗
    // 只在血量极低或多怪围攻时才逃跑

    // 血量 <25%（criticalHealth）：转身逃跑，脱战后 eat/drink 自动恢复
    triggers.push_back(
        new TriggerNode("critical health", { NextAction("run from target", ACTION_EMERGENCY + 5) }));
    // 多怪(>2) 且血量 <50%：转身逃跑
    triggers.push_back(
        new TriggerNode("multiple attackers low health", { NextAction("run from target", ACTION_EMERGENCY + 4) }));
}

void FleeFromAddsStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(
        new TriggerNode("has nearest adds", { NextAction("runaway", 50.0f) }));
}
