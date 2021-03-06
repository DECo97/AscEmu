/*
Copyright (c) 2014-2019 AscEmu Team <http://www.ascemu.org>
This file is released under the MIT license. See README-MIT for more information.
*/

#include "Setup.h"
#include "Server/Script/ScriptSetup.h"
#include "Server/Script/ScriptMgr.h"

extern "C" SCRIPT_DECL void _exp_set_serverstate_singleton(ServerState* state)
{
    ServerState::instance(state);
}

extern "C" SCRIPT_DECL uint32 _exp_get_script_type()
{
    return SCRIPT_TYPE_MISC;
}

extern "C" SCRIPT_DECL void _exp_script_register(ScriptMgr* mgr)
{
    SetupDalaranGossip(mgr);
    SetupInnkeepers(mgr);
    SetupTrainerScript(mgr);
    SetupMulgoreGossip(mgr);
    SetupShattrathGossip(mgr);
    SetupTanarisGossip(mgr);
    SetupMoongladeGossip(mgr);
    SetupStormwindGossip(mgr);
    SetupTheramoreGossip(mgr);
    SetupTeldrassilGossip(mgr);
    SetupBoreanTundraGossip(mgr);
    SetupWyrmrestTempleGossip(mgr);
    SetupXpEliminatorGossip(mgr);
}

#ifdef WIN32
BOOL APIENTRY DllMain(HANDLE /*hModule*/, DWORD  /*ul_reason_for_call*/, LPVOID /*lpReserved*/)
{
    return TRUE;
}
#endif
