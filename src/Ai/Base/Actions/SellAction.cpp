/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#include "SellAction.h"

#include "Event.h"
#include "ItemPackets.h"
#include "ItemUsageValue.h"
#include "ItemVisitors.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "PositionValue.h"
#include "ObjectMgr.h"

class SellItemsVisitor : public IterateItemsVisitor
{
public:
    SellItemsVisitor(SellAction* action) : IterateItemsVisitor(), action(action) {}

    bool Visit(Item* item) override
    {
        action->Sell(item);
        return true;
    }

private:
    SellAction* action;
};

class SellGrayItemsVisitor : public SellItemsVisitor
{
public:
    SellGrayItemsVisitor(SellAction* action) : SellItemsVisitor(action) {}

    bool Visit(Item* item) override
    {
        if (item->GetTemplate()->Quality != ITEM_QUALITY_POOR)
            return true;

        return SellItemsVisitor::Visit(item);
    }
};

class SellVendorItemsVisitor : public SellItemsVisitor
{
public:
    SellVendorItemsVisitor(SellAction* action, AiObjectContext* con) : SellItemsVisitor(action) { context = con; }

    AiObjectContext* context;

    bool Visit(Item* item) override
    {
        ItemUsage usage = context->GetValue<ItemUsage>("item usage", item->GetEntry())->Get();
        if (usage != ITEM_USAGE_VENDOR && usage != ITEM_USAGE_AH)
            return true;

        return SellItemsVisitor::Visit(item);
    }
};

// 按配置品质过滤：保留任务物品，按 conf/GM命令决定卖白/绿/蓝
class BagFullSellItemsVisitor : public SellItemsVisitor
{
public:
    BagFullSellItemsVisitor(SellAction* action) : SellItemsVisitor(action) {}

    bool Visit(Item* item) override
    {
        ItemTemplate const* proto = item->GetTemplate();
        if (!proto)
            return true;

        // 任务物品永远保留
        if (proto->Class == ITEM_CLASS_QUEST)
            return true;

        uint32 quality = proto->Quality;

        if (quality == ITEM_QUALITY_POOR)
            return SellItemsVisitor::Visit(item);

        if (quality == ITEM_QUALITY_NORMAL && sPlayerbotAIConfig.sellWhiteItems)
            return SellItemsVisitor::Visit(item);

        if (quality == ITEM_QUALITY_UNCOMMON && sPlayerbotAIConfig.sellGreenItems)
            return SellItemsVisitor::Visit(item);

        if (quality == ITEM_QUALITY_RARE && sPlayerbotAIConfig.sellBlueItems)
            return SellItemsVisitor::Visit(item);

        return true;
    }
};

// ─── SellAction ──────────────────────────────────────────────────────────────

bool SellAction::Execute(Event event)
{
    std::string const text = event.getParam();
    if (text == "gray" || text == "*")
    {
        SellGrayItemsVisitor visitor(this);
        IterateItems(&visitor);
        return true;
    }

    if (text == "vendor")
    {
        SellVendorItemsVisitor visitor(this, context);
        IterateItems(&visitor);
        return true;
    }

    if (text != "")
    {
        std::vector<Item*> items = parseItems(text, ITERATE_ITEMS_IN_BAGS);
        for (Item* item : items)
            Sell(item);
        return true;
    }

    botAI->TellError("Usage: s gray/*/vendor/[item link]");
    return false;
}

void SellAction::Sell(FindItemVisitor* visitor)
{
    IterateItems(visitor);
    std::vector<Item*> items = visitor->GetResult();
    for (Item* item : items)
        Sell(item);
}

void SellAction::Sell(Item* item)
{
    std::ostringstream out;

    GuidVector vendors = botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest npcs")->Get();

    for (ObjectGuid const vendorguid : vendors)
    {
        Creature* pCreature = bot->GetNPCIfCanInteractWith(vendorguid, UNIT_NPC_FLAG_VENDOR);
        if (!pCreature)
            continue;

        ObjectGuid itemguid = item->GetGUID();
        uint32 count = item->GetCount();
        uint32 botMoney = bot->GetMoney();

        WorldPacket p(CMSG_SELL_ITEM);
        p << vendorguid << itemguid << count;

        WorldPackets::Item::SellItem nicePacket(std::move(p));
        nicePacket.Read();
        bot->GetSession()->HandleSellItemOpcode(nicePacket);

        if (botAI->HasCheat(BotCheatMask::gold))
            bot->SetMoney(botMoney);

        out << "Selling " << chat->FormatItem(item->GetTemplate());
        botAI->TellMaster(out);
        bot->PlayDistanceSound(120);
        break;
    }
}

// ─── TravelToVendorAction ─────────────────────────────────────────────────────

bool TravelToVendorAction::Execute(Event /*event*/)
{
    // 只在视野范围（nearest npcs）内找商人并走过去
    GuidVector npcs = AI_VALUE(GuidVector, "nearest npcs");

    Creature* nearest = nullptr;
    float nearestDist = 999999.0f;
    for (ObjectGuid const guid : npcs)
    {
        Creature* c = bot->GetMap()->GetCreature(guid);
        if (!c || !c->IsAlive() || !(c->GetNpcFlags() & UNIT_NPC_FLAG_VENDOR))
            continue;
        float dist = bot->GetDistance(c);
        if (dist < nearestDist)
        {
            nearestDist = dist;
            nearest = c;
        }
    }

    if (!nearest)
        return false;

    // 出发前保存当前位置，卖完货回来继续打怪
    PositionMap& posMap = context->GetValue<PositionMap&>("position")->Get();
    if (!posMap["sell_return"].isSet())
    {
        posMap["sell_return"].Set(bot->GetPositionX(), bot->GetPositionY(),
                                  bot->GetPositionZ(), bot->GetMapId());
    }

    return MoveTo(nearest, INTERACTION_DISTANCE);
}

bool TravelToVendorAction::isUseful()
{
    if (bot->IsInCombat())
        return false;

    if (AI_VALUE(uint8, "bag space") < sPlayerbotAIConfig.bagFullSellThreshold)
        return false;

    // 已经在商人交互范围内：让 BagFullSellAction 处理，不需要移动
    GuidVector npcs = AI_VALUE(GuidVector, "nearest npcs");
    for (ObjectGuid const guid : npcs)
    {
        if (bot->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_VENDOR))
            return false;
    }

    return true;
}

// ─── BagFullSellAction ────────────────────────────────────────────────────────

bool BagFullSellAction::Execute(Event /*event*/)
{
    GuidVector npcs = AI_VALUE(GuidVector, "nearest npcs");
    for (ObjectGuid const guid : npcs)
    {
        Creature* vendor = bot->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_VENDOR);
        if (!vendor)
            continue;

        BagFullSellItemsVisitor visitor(this);
        IterateItems(&visitor);

        // 卖货完成，标记"需要返回"：把 sell_return 位置改名为 sell_done
        // ReturnFromVendorAction 检测到 sell_done 后寻路回去
        PositionMap& posMap = context->GetValue<PositionMap&>("position")->Get();
        if (posMap["sell_return"].isSet())
        {
            posMap["sell_done"] = posMap["sell_return"];
            posMap["sell_return"].Reset();
        }

        return true;
    }
    return false;
}

bool BagFullSellAction::isUseful()
{
    if (bot->IsInCombat())
        return false;

    if (AI_VALUE(uint8, "bag space") < sPlayerbotAIConfig.bagFullSellThreshold)
        return false;

    GuidVector npcs = AI_VALUE(GuidVector, "nearest npcs");
    for (ObjectGuid const guid : npcs)
    {
        if (bot->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_VENDOR))
            return true;
    }
    return false;
}

// ─── ReturnFromVendorAction ────────────────────────────────────────────────────

bool ReturnFromVendorAction::Execute(Event /*event*/)
{
    PositionMap& posMap = context->GetValue<PositionMap&>("position")->Get();
    PositionInfo returnPos = posMap["sell_done"];

    if (!returnPos.isSet())
        return false;

    // 到达后清除标记
    if (bot->GetDistance(returnPos.x, returnPos.y, returnPos.z) < 5.0f)
    {
        posMap["sell_done"].Reset();
        return true;
    }

    return MoveTo(returnPos.mapId, returnPos.x, returnPos.y, returnPos.z);
}

bool ReturnFromVendorAction::isUseful()
{
    if (bot->IsInCombat())
        return false;

    // 有 sell_done 标记且背包不再满
    PositionMap& posMap = context->GetValue<PositionMap&>("position")->Get();
    return posMap["sell_done"].isSet() &&
           AI_VALUE(uint8, "bag space") < sPlayerbotAIConfig.bagFullSellThreshold;
}

// ─── TravelToAmmoVendorAction ─────────────────────────────────────────────────

bool TravelToAmmoVendorAction::Execute(Event /*event*/)
{
    // 先在视野范围内找弹药商（卖弓箭/子弹的商人）
    GuidVector npcs = AI_VALUE(GuidVector, "nearest npcs");
    for (ObjectGuid const guid : npcs)
    {
        Creature* c = bot->GetMap()->GetCreature(guid);
        if (!c || !c->IsAlive() || !(c->GetNpcFlags() & UNIT_NPC_FLAG_VENDOR))
            continue;

        // 检查这个商人是否卖弹药
        VendorItemData const* items = c->GetVendorItems();
        if (!items) continue;
        for (uint32 i = 0; i < items->GetItemCount(); ++i)
        {
            ItemTemplate const* proto = sObjectMgr->GetItemTemplate(items->GetItem(i)->item);
            if (proto && proto->Class == ITEM_CLASS_PROJECTILE)
                return MoveTo(c, INTERACTION_DISTANCE);
        }
    }

    // 视野内没有，用 SQL 查同 map 上最近的弹药商
    float bx = bot->GetPositionX();
    float by = bot->GetPositionY();
    uint32 mapId = bot->GetMapId();

    // 先查有卖弹药的商人所在坐标
    QueryResult result = WorldDatabase.Query(
        "SELECT c.position_x, c.position_y, c.position_z "
        "FROM creature c "
        "JOIN creature_template ct ON ct.entry = c.id1 "
        "JOIN npc_vendor nv ON nv.entry = ct.entry "
        "JOIN item_template it ON it.entry = nv.item "
        "WHERE c.map = {} AND (ct.npcflag & 128) != 0 AND it.class = 6 "
        "ORDER BY (POW(c.position_x - {}, 2) + POW(c.position_y - {}, 2)) ASC "
        "LIMIT 1",
        mapId, bx, by);

    if (!result)
        return false;

    Field* fields = result->Fetch();
    return MoveTo(mapId, fields[0].Get<float>(), fields[1].Get<float>(), fields[2].Get<float>());
}

bool TravelToAmmoVendorAction::isUseful()
{
    if (bot->IsInCombat() || bot->getClass() != CLASS_HUNTER)
        return false;

    if (!botAI->HasAdvancedGrindPermission())
        return false;

    // 只有弹药耗尽时才触发
    if (AI_VALUE2(uint32, "item count", "ammo") > 0)
        return false;

    // 已经在弹药商旁边了，不需要移动
    GuidVector npcs = AI_VALUE(GuidVector, "nearest npcs");
    for (ObjectGuid const guid : npcs)
    {
        Creature* c = bot->GetMap()->GetCreature(guid);
        if (!c || !c->IsAlive() || !(c->GetNpcFlags() & UNIT_NPC_FLAG_VENDOR))
            continue;
        VendorItemData const* items = c->GetVendorItems();
        if (!items) continue;
        for (uint32 i = 0; i < items->GetItemCount(); ++i)
        {
            ItemTemplate const* proto = sObjectMgr->GetItemTemplate(items->GetItem(i)->item);
            if (proto && proto->Class == ITEM_CLASS_PROJECTILE)
                return false;  // 已在弹药商旁边，由 BuyAmmoAction 处理
        }
    }
    return true;
}

// ─── BuyAmmoAction ───────────────────────────────────────────────────────────

uint32 BuyAmmoAction::CalcAmmoBuyCount(ItemTemplate const* proto) const
{
    uint32 freeBagSlots = 0;
    for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag)
    {
        const Bag* pBag = (Bag*)bot->GetItemByPos(INVENTORY_SLOT_BAG_0, bag);
        if (pBag) freeBagSlots += pBag->GetFreeSlots();
    }
    // 背包默认16格
    for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
        if (!bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot)) ++freeBagSlots;

    uint32 maxStack = proto->GetMaxStackSize();
    uint32 canAfford = bot->GetMoney() / proto->BuyPrice;
    uint32 canCarry  = freeBagSlots * maxStack;

    // 购买 4 叠（够打一段时间），但不超过金钱/背包限制
    uint32 wantStacks = 4;
    uint32 wantCount  = wantStacks * maxStack;
    return std::min({wantCount, canAfford, canCarry});
}

bool BuyAmmoAction::Execute(Event /*event*/)
{
    if (bot->getClass() != CLASS_HUNTER)
        return false;

    Item* ranged = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_RANGED);
    if (!ranged) return false;

    uint32 requiredSubClass = 0;
    switch (ranged->GetTemplate()->SubClass)
    {
        case ITEM_SUBCLASS_WEAPON_GUN:       requiredSubClass = ITEM_SUBCLASS_BULLET; break;
        case ITEM_SUBCLASS_WEAPON_BOW:
        case ITEM_SUBCLASS_WEAPON_CROSSBOW:  requiredSubClass = ITEM_SUBCLASS_ARROW;  break;
        default: return false;
    }

    GuidVector npcs = AI_VALUE(GuidVector, "nearest npcs");
    for (ObjectGuid const guid : npcs)
    {
        Creature* vendor = bot->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_VENDOR);
        if (!vendor) continue;

        VendorItemData const* vItems = vendor->GetVendorItems();
        if (!vItems) continue;

        // 找到最高DPS的对应弹药
        ItemTemplate const* bestProto = nullptr;
        uint32 bestDPS = 0;
        for (uint32 i = 0; i < vItems->GetItemCount(); ++i)
        {
            ItemTemplate const* proto = sObjectMgr->GetItemTemplate(vItems->GetItem(i)->item);
            if (!proto || proto->Class != ITEM_CLASS_PROJECTILE) continue;
            if (proto->SubClass != requiredSubClass) continue;
            if (bot->CanUseItem(proto) != EQUIP_ERR_OK) continue;

            uint32 dps = (proto->Damage[0].DamageMin + proto->Damage[0].DamageMax) * 1000 / 2;
            if (dps > bestDPS)
            {
                bestDPS  = dps;
                bestProto = proto;
            }
        }

        if (!bestProto) continue;

        uint32 buyCount = CalcAmmoBuyCount(bestProto);
        if (buyCount == 0) return false;

        uint32 stackSize = bestProto->GetMaxStackSize();
        uint32 stacks    = (buyCount + stackSize - 1) / stackSize;
        bool   bought    = false;

        for (uint32 i = 0; i < vItems->GetItemCount(); ++i)
        {
            if (vItems->GetItem(i)->item != bestProto->ItemId) continue;
            for (uint32 s = 0; s < stacks; ++s)
            {
                uint32 before = bot->GetItemCount(bestProto->ItemId, false);
                bot->BuyItemFromVendorSlot(guid, i, bestProto->ItemId, stackSize, NULL_BAG, NULL_SLOT);
                if (bot->GetItemCount(bestProto->ItemId, false) > before)
                    bought = true;
                else
                    break;
            }
            break;
        }

        if (bought)
        {
            std::ostringstream out;
            out << "Bought ammo: " << ChatHelper::FormatItem(bestProto);
            botAI->TellMaster(out.str());
            return true;
        }
    }
    return false;
}

bool BuyAmmoAction::isUseful()
{
    if (bot->IsInCombat() || bot->getClass() != CLASS_HUNTER)
        return false;

    if (!botAI->HasAdvancedGrindPermission())
        return false;

    if (AI_VALUE2(uint32, "item count", "ammo") > 0)
        return false;

    // 必须已在弹药商交互范围内
    GuidVector npcs = AI_VALUE(GuidVector, "nearest npcs");
    for (ObjectGuid const guid : npcs)
    {
        Creature* vendor = bot->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_VENDOR);
        if (!vendor) continue;
        VendorItemData const* items = vendor->GetVendorItems();
        if (!items) continue;
        for (uint32 i = 0; i < items->GetItemCount(); ++i)
        {
            ItemTemplate const* proto = sObjectMgr->GetItemTemplate(items->GetItem(i)->item);
            if (proto && proto->Class == ITEM_CLASS_PROJECTILE)
                return true;
        }
    }
    return false;
}
