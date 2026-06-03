/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#include "AvailableLootValue.h"

#include "CellImpl.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "LootObjectStack.h"
#include "Playerbots.h"
#include "ServerFacade.h"

AvailableLootValue::AvailableLootValue(PlayerbotAI* botAI, std::string const name)
    : ManualSetValue<LootObjectStack*>(botAI, nullptr, name)
{
    value = new LootObjectStack(botAI->GetBot());
}

AvailableLootValue::~AvailableLootValue() { delete value; }

LootTargetValue::LootTargetValue(PlayerbotAI* botAI, std::string const name)
    : ManualSetValue<LootObject>(botAI, LootObject(), name)
{
}

bool CanLootValue::Calculate()
{
    LootObject loot = AI_VALUE(LootObject, "loot target");
    if (loot.IsEmpty() || !loot.GetWorldObject(bot) || !loot.IsLootPossible(bot))
        return false;

    if (!ServerFacade::instance().IsDistanceLessOrEqualThan(AI_VALUE2(float, "distance", "loot target"), INTERACTION_DISTANCE - 2))
        return false;

    // 箱子/矿/草药：检查周围是否有敌对怪物，而不是用 IsInCombat
    // 这样即使远处还在战斗，只要箱子附近安全就可以开
    WorldObject* wo = loot.GetWorldObject(bot);
    if (wo && wo->GetTypeId() == TYPEID_GAMEOBJECT)
    {
        // 以箱子位置为中心，检查 25 码内是否有存活的敌对单位
        float safeRange = 25.0f;
        std::list<Unit*> nearbyUnits;
        Acore::AnyUnfriendlyUnitInObjectRangeCheck u_check(bot, bot, safeRange);
        Acore::UnitListSearcher<Acore::AnyUnfriendlyUnitInObjectRangeCheck> searcher(bot, nearbyUnits, u_check);
        Cell::VisitObjects(wo, searcher, safeRange);

        for (Unit* unit : nearbyUnits)
        {
            // 只检查箱子 25 码内的存活怪物
            if (unit && unit->IsAlive() && unit->IsCreature() &&
                wo->GetDistance(unit) <= safeRange)
                return false;
        }
    }

    return true;
}
