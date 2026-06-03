/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#include "GenericDruidStrategy.h"

#include "Playerbots.h"

class GenericDruidStrategyActionNodeFactory : public NamedObjectFactory<ActionNode>
{
public:
    GenericDruidStrategyActionNodeFactory()
    {
        creators["melee"] = &melee;
        creators["caster form"] = &caster_form;
        creators["cure poison"] = &cure_poison;
        creators["cure poison on party"] = &cure_poison_on_party;
        creators["abolish poison"] = &abolish_poison;
        creators["abolish poison on party"] = &abolish_poison_on_party;
        creators["rebirth"] = &rebirth;
        creators["entangling roots on cc"] = &entangling_roots_on_cc;
        creators["innervate"] = &innervate;
        creators["regrowth"] = &regrowth;
        creators["healing touch"] = &healing_touch;
        creators["rejuvenation"] = &rejuvenation;
    }

private:
    static ActionNode* melee([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode("melee",
                              /*P*/ {},
                              /*A*/ {},
                              /*C*/ {});
    }

    static ActionNode* caster_form([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode("caster form",
                              /*P*/ {},
                              /*A*/ {},
                              /*C*/ {});
    }

    static ActionNode* cure_poison([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode("cure poison",
                              /*P*/ {},
                              /*A*/ {},
                              /*C*/ {});
    }

    static ActionNode* cure_poison_on_party([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode("cure poison on party",
                              /*P*/ {},
                              /*A*/ {},
                              /*C*/ {});
    }

    static ActionNode* abolish_poison([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode("abolish poison",
                              /*P*/ {},
                              /*A*/ {},
                              /*C*/ {});
    }

    static ActionNode* abolish_poison_on_party([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode("abolish poison on party",
                              /*P*/ {},
                              /*A*/ {},
                              /*C*/ {});
    }

    static ActionNode* rebirth([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode("rebirth",
                              /*P*/ {},
                              /*A*/ {},
                              /*C*/ {});
    }

    static ActionNode* entangling_roots_on_cc([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode("entangling roots on cc",
                              /*P*/ { NextAction("caster form") },
                              /*A*/ {},
                              /*C*/ {});
    }

    static ActionNode* innervate([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode("innervate",
                              /*P*/ {},
                              /*A*/ { NextAction("mana potion") },
                              /*C*/ {});
    }

    // 人形状态直接施法；猫/熊形时先切人形再施法（P=前置，C=后续）
    static ActionNode* regrowth([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode("regrowth",
                              /*P*/ { NextAction("caster form") },
                              /*A*/ { NextAction("healing touch") },
                              /*C*/ {});
    }

    static ActionNode* healing_touch([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode("healing touch",
                              /*P*/ { NextAction("caster form") },
                              /*A*/ {},
                              /*C*/ {});
    }

    static ActionNode* rejuvenation([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode("rejuvenation",
                              /*P*/ { NextAction("caster form") },
                              /*A*/ {},
                              /*C*/ {});
    }
};

GenericDruidStrategy::GenericDruidStrategy(PlayerbotAI* botAI) : CombatStrategy(botAI)
{
    actionNodeFactories.Add(new GenericDruidStrategyActionNodeFactory());
}

void GenericDruidStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    CombatStrategy::InitTriggers(triggers);

    triggers.push_back(
        new TriggerNode("low health", { NextAction("barkskin", ACTION_HIGH + 7) }));

    // 血量低：自愈（人形可直接施法；猫/熊形会先切人形）
    // critical health(25%)：regrowth（直接治疗）或 healing touch
    triggers.push_back(new TriggerNode("critical health",
        { NextAction("regrowth", ACTION_CRITICAL_HEAL + 3),
          NextAction("healing touch", ACTION_CRITICAL_HEAL + 2) }));
    // low health(45%)：回春（HoT，先挂上持续回血）
    triggers.push_back(new TriggerNode("low health",
        { NextAction("rejuvenation", ACTION_MEDIUM_HEAL + 2) }));

    triggers.push_back(new TriggerNode("combat party member dead",
                                       { NextAction("rebirth", ACTION_HIGH + 9) }));
    triggers.push_back(new TriggerNode("being attacked",
                                       { NextAction("nature's grasp", ACTION_HIGH + 1) }));
    triggers.push_back(new TriggerNode("new pet", { NextAction("set pet stance", 60.0f) }));
}

void DruidCureStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(
        new TriggerNode("party member cure poison",
                        { NextAction("abolish poison on party", ACTION_DISPEL + 1) }));

    triggers.push_back(
        new TriggerNode("party member remove curse",
                        { NextAction("remove curse on party", ACTION_DISPEL + 7) }));

}

void DruidBoostStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode(
        "nature's swiftness", { NextAction("nature's swiftness", ACTION_HIGH + 9) }));
}

void DruidCcStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode(
        "entangling roots", { NextAction("entangling roots on cc", ACTION_HIGH + 2) }));
    triggers.push_back(new TriggerNode(
        "entangling roots kite", { NextAction("entangling roots", ACTION_HIGH + 2) }));
    triggers.push_back(new TriggerNode(
        "hibernate", { NextAction("hibernate on cc", ACTION_HIGH + 3) }));
}

void DruidHealerDpsStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(
        new TriggerNode("healer should attack",
                        {
                            NextAction("cancel tree form", ACTION_DEFAULT + 0.4f),
                            NextAction("moonfire", ACTION_DEFAULT + 0.3f),
                            NextAction("wrath", ACTION_DEFAULT + 0.2f),
                            NextAction("starfire", ACTION_DEFAULT + 0.1f),
}));

}
