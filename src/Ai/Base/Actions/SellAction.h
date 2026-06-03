/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#ifndef _PLAYERBOT_SELLACTION_H
#define _PLAYERBOT_SELLACTION_H

#include "InventoryAction.h"
#include "MovementActions.h"
#include "ObjectGuid.h"

class FindItemVisitor;
class Item;
class PlayerbotAI;

class SellAction : public InventoryAction
{
public:
    SellAction(PlayerbotAI* botAI, std::string const name = "sell") : InventoryAction(botAI, name) {}

    bool Execute(Event event) override;
    void Sell(FindItemVisitor* visitor);
    void Sell(Item* item);
};

// 背包满时寻路到视野内最近商人（状态机第一步）
// 出发前保存当前位置到 position["sell_return"]，卖完后回到原地
class TravelToVendorAction : public MovementAction
{
public:
    TravelToVendorAction(PlayerbotAI* botAI) : MovementAction(botAI, "travel to vendor") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

// 背包满时按配置品质卖货给商人（状态机第二步）
class BagFullSellAction : public SellAction
{
public:
    BagFullSellAction(PlayerbotAI* botAI) : SellAction(botAI, "bag full sell") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

// 卖货完成后回到出发点（状态机第三步）
class ReturnFromVendorAction : public MovementAction
{
public:
    ReturnFromVendorAction(PlayerbotAI* botAI) : MovementAction(botAI, "return from vendor") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

// 猎人无弹药：寻路到最近弹药商并购买弹药
// 流程：无弹药 → 先卖货（背包满时） → 走到弹药商 → 购买 → 恢复
class TravelToAmmoVendorAction : public MovementAction
{
public:
    TravelToAmmoVendorAction(PlayerbotAI* botAI) : MovementAction(botAI, "travel to ammo vendor") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

class BuyAmmoAction : public InventoryAction
{
public:
    BuyAmmoAction(PlayerbotAI* botAI) : InventoryAction(botAI, "buy ammo") {}

    bool Execute(Event event) override;
    bool isUseful() override;

private:
    // 根据背包空间和金钱计算购买数量
    uint32 CalcAmmoBuyCount(ItemTemplate const* proto) const;
};

#endif
