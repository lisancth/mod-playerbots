/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "BattleGroundTactics.h"
#include "Chat.h"
#include "GuildTaskMgr.h"
#include "PerfMonitor.h"
#include "PlayerbotAIConfig.h"
#include "PlayerbotMgr.h"
#include "RandomPlayerbotMgr.h"
#include "ScriptMgr.h"

using namespace Acore::ChatCommands;

class playerbots_commandscript : public CommandScript
{
public:
    playerbots_commandscript() : CommandScript("playerbots_commandscript") {}

    static bool HandleDrinkModeCommand(ChatHandler* handler, char const* args)
    {
        if (!args || !*args)
        {
            bool foodCheatOn = (sPlayerbotAIConfig.botCheatMask & (uint32)BotCheatMask::food) != 0;
            handler->PSendSysMessage("Bot drinkmode: %s (food cheat is %s)",
                foodCheatOn ? "auto (no items needed)" : "real items required",
                foodCheatOn ? "ON" : "OFF");
            handler->PSendSysMessage("Usage: .playerbots drinkmode on|off");
            return true;
        }

        if (!strcasecmp(args, "on"))
        {
            // "on" = 真实喝水模式：关闭 food cheat，bot 需要背包有食物/水
            sPlayerbotAIConfig.botCheatMask &= ~(uint32)BotCheatMask::food;
            handler->PSendSysMessage("[DrinkMode] Real items mode ON: bots now require food/water in bags.");
            LOG_INFO("playerbots", "GM {} set bot drinkmode to real-items (food cheat OFF)",
                handler->GetSession() ? handler->GetSession()->GetPlayerName() : "Console");
        }
        else if (!strcasecmp(args, "off"))
        {
            // "off" = 自动模式：开启 food cheat，bot 无需物品自动回血回蓝
            sPlayerbotAIConfig.botCheatMask |= (uint32)BotCheatMask::food;
            handler->PSendSysMessage("[DrinkMode] Auto mode ON: bots regenerate without needing food/water.");
            LOG_INFO("playerbots", "GM {} set bot drinkmode to auto (food cheat ON)",
                handler->GetSession() ? handler->GetSession()->GetPlayerName() : "Console");
        }
        else
        {
            handler->PSendSysMessage("Usage: .playerbots drinkmode on|off");
            handler->PSendSysMessage("  on  = real items required (disable food cheat)");
            handler->PSendSysMessage("  off = auto regenerate (enable food cheat, default)");
            return false;
        }

        return true;
    }

    // .playerbots sellmode [gray|white|green|blue|status]
    // gray   = 只卖灰色（最保守）
    // white  = 卖灰+白
    // green  = 卖灰+白+绿
    // blue   = 卖灰+白+绿+蓝
    // status = 查看当前设置
    static bool HandleSellModeCommand(ChatHandler* handler, char const* args)
    {
        if (!args || !*args || !strcasecmp(args, "status"))
        {
            handler->PSendSysMessage("[SellMode] Bag threshold: %u%%  Sell: gray=%s white=%s green=%s blue=%s",
                sPlayerbotAIConfig.bagFullSellThreshold,
                sPlayerbotAIConfig.sellGrayItems  ? "ON" : "OFF",
                sPlayerbotAIConfig.sellWhiteItems ? "ON" : "OFF",
                sPlayerbotAIConfig.sellGreenItems ? "ON" : "OFF",
                sPlayerbotAIConfig.sellBlueItems  ? "ON" : "OFF");
            handler->PSendSysMessage("Usage: .playerbots sellmode gray|white|green|blue|status");
            return true;
        }

        // 灰色始终开启，按参数叠加
        sPlayerbotAIConfig.sellGrayItems  = true;
        sPlayerbotAIConfig.sellWhiteItems = false;
        sPlayerbotAIConfig.sellGreenItems = false;
        sPlayerbotAIConfig.sellBlueItems  = false;

        if (!strcasecmp(args, "gray"))
        {
            handler->PSendSysMessage("[SellMode] Selling: gray only.");
        }
        else if (!strcasecmp(args, "white"))
        {
            sPlayerbotAIConfig.sellWhiteItems = true;
            handler->PSendSysMessage("[SellMode] Selling: gray + white.");
        }
        else if (!strcasecmp(args, "green"))
        {
            sPlayerbotAIConfig.sellWhiteItems = true;
            sPlayerbotAIConfig.sellGreenItems = true;
            handler->PSendSysMessage("[SellMode] Selling: gray + white + green.");
        }
        else if (!strcasecmp(args, "blue"))
        {
            sPlayerbotAIConfig.sellWhiteItems = true;
            sPlayerbotAIConfig.sellGreenItems = true;
            sPlayerbotAIConfig.sellBlueItems  = true;
            handler->PSendSysMessage("[SellMode] Selling: gray + white + green + blue.");
        }
        else
        {
            handler->PSendSysMessage("Usage: .playerbots sellmode gray|white|green|blue|status");
            return false;
        }

        LOG_INFO("playerbots", "GM {} changed bot sellmode to: {}",
            handler->GetSession() ? handler->GetSession()->GetPlayerName() : "Console", args);
        return true;
    }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable playerbotsDebugCommandTable = {
            {"bg", HandleDebugBGCommand, SEC_GAMEMASTER, Console::Yes},
        };

        static ChatCommandTable playerbotsAccountCommandTable = {
            {"setKey", HandleSetSecurityKeyCommand, SEC_PLAYER, Console::No},
            {"link", HandleLinkAccountCommand, SEC_PLAYER, Console::No},
            {"linkedAccounts", HandleViewLinkedAccountsCommand, SEC_PLAYER, Console::No},
            {"unlink", HandleUnlinkAccountCommand, SEC_PLAYER, Console::No},
        };

        static ChatCommandTable playerbotsCommandTable = {
            {"bot", HandlePlayerbotCommand, SEC_PLAYER, Console::No},
            {"gtask", HandleGuildTaskCommand, SEC_GAMEMASTER, Console::Yes},
            {"pmon", HandlePerfMonCommand, SEC_GAMEMASTER, Console::Yes},
            {"rndbot", HandleRandomPlayerbotCommand, SEC_GAMEMASTER, Console::Yes},
            {"drinkmode", HandleDrinkModeCommand, SEC_GAMEMASTER, Console::Yes},
            {"sellmode", HandleSellModeCommand, SEC_GAMEMASTER, Console::Yes},
            {"debug", playerbotsDebugCommandTable},
            {"account", playerbotsAccountCommandTable},
        };

        static ChatCommandTable commandTable = {
            {"playerbots", playerbotsCommandTable},
        };

        return commandTable;
    }

    static bool HandlePlayerbotCommand(ChatHandler* handler, char const* args)
    {
        return PlayerbotMgr::HandlePlayerbotMgrCommand(handler, args);
    }

    static bool HandleRandomPlayerbotCommand(ChatHandler* handler, char const* args)
    {
        return RandomPlayerbotMgr::HandlePlayerbotConsoleCommand(handler, args);
    }

    static bool HandleGuildTaskCommand(ChatHandler* handler, char const* args)
    {
        return GuildTaskMgr::HandleConsoleCommand(handler, args);
    }

    static bool HandlePerfMonCommand(ChatHandler* handler, char const* args)
    {
        if (!strcmp(args, "reset"))
        {
            sPerfMonitor.Reset();
            return true;
        }

        if (!strcmp(args, "tick"))
        {
            sPerfMonitor.PrintStats(true, false);
            return true;
        }

        if (!strcmp(args, "stack"))
        {
            sPerfMonitor.PrintStats(false, true);
            return true;
        }

        if (!strcmp(args, "toggle"))
        {
            sPlayerbotAIConfig.perfMonEnabled = !sPlayerbotAIConfig.perfMonEnabled;
            if (sPlayerbotAIConfig.perfMonEnabled)
                LOG_INFO("playerbots", "Performance monitor enabled");
            else
                LOG_INFO("playerbots", "Performance monitor disabled");
            return true;
        }

        sPerfMonitor.PrintStats();
        return true;
    }

    static bool HandleDebugBGCommand(ChatHandler* handler, char const* args)
    {
        return BGTactics::HandleConsoleCommand(handler, args);
    }

    static bool HandleSetSecurityKeyCommand(ChatHandler* handler, char const* args)
    {
        if (!args || !*args)
        {
            handler->PSendSysMessage("Usage: .playerbots account setKey <securityKey>");
            return false;
        }

        Player* player = handler->GetSession()->GetPlayer();
        std::string key = args;

        PlayerbotMgr* mgr = PlayerbotsMgr::instance().GetPlayerbotMgr(player);
        if (mgr)
        {
            mgr->HandleSetSecurityKeyCommand(player, key);
            return true;
        }
        else
        {
            handler->PSendSysMessage("PlayerbotMgr instance not found.");
            return false;
        }
    }

    static bool HandleLinkAccountCommand(ChatHandler* handler, char const* args)
    {
        if (!args || !*args)
            return false;

        char* accountName = strtok((char*)args, " ");
        char* key = strtok(nullptr, " ");

        if (!accountName || !key)
        {
            handler->PSendSysMessage("Usage: .playerbots account link <accountName> <securityKey>");
            return false;
        }

        Player* player = handler->GetSession()->GetPlayer();

        PlayerbotMgr* mgr = PlayerbotsMgr::instance().GetPlayerbotMgr(player);
        if (mgr)
        {
            mgr->HandleLinkAccountCommand(player, accountName, key);
            return true;
        }
        else
        {
            handler->PSendSysMessage("PlayerbotMgr instance not found.");
            return false;
        }
    }

    static bool HandleViewLinkedAccountsCommand(ChatHandler* handler, char const* /*args*/)
    {
        Player* player = handler->GetSession()->GetPlayer();

        PlayerbotMgr* mgr = PlayerbotsMgr::instance().GetPlayerbotMgr(player);
        if (mgr)
        {
            mgr->HandleViewLinkedAccountsCommand(player);
            return true;
        }
        else
        {
            handler->PSendSysMessage("PlayerbotMgr instance not found.");
            return false;
        }
    }

    static bool HandleUnlinkAccountCommand(ChatHandler* handler, char const* args)
    {
        if (!args || !*args)
            return false;

        char* accountName = strtok((char*)args, " ");
        if (!accountName)
        {
            handler->PSendSysMessage("Usage: .playerbots account unlink <accountName>");
            return false;
        }

        Player* player = handler->GetSession()->GetPlayer();

        PlayerbotMgr* mgr = PlayerbotsMgr::instance().GetPlayerbotMgr(player);
        if (mgr)
        {
            mgr->HandleUnlinkAccountCommand(player, accountName);
            return true;
        }
        else
        {
            handler->PSendSysMessage("PlayerbotMgr instance not found.");
            return false;
        }
    }

};

void AddPlayerbotsCommandscripts() { new playerbots_commandscript(); }
