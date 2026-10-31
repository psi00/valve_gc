#include "stdafx.h"
#include "game_profile.h"
#include "config.h"

const GameProfile g_profileCSGO = {
    .mode                    = GameMode::CSGO,
    .appId                   = 4465480,
    .engineModule            = "engine",
    .gameEventManagerVersion = "GAMEEVENTSMANAGER002",
    .engineClientVersion     = "VEngineClient014",
    .requiredAppIdVersion    = 13857,
    .requiredAppIdVersion2   = 13862,
    .pricesheetVersion       = 1680057676,
    .activeTournamentEventId = 20,
    .localPlayerControllerOffset = 0,
    .networkGameClientOffset     = 0,
    .networkGameClientLocalPlayer = 0,
};

// Version data re-pinned from steam.inf; see the "Pinned:" line below.
// The build number is the PatchVersion with the dots stripped, which is what
// CS2 writes to CDemoFileHeader.patch_version (demo.proto field 2, the slot
// CS:GO called network_protocol) and what it compares against on demo playback,
// so requiredAppIdVersion tracks it.
// e.g. PatchVersion 1.41.6.5 -> 14165, the value read from a June 2026 demo.
//
// requiredAppIdVersion2 is demo.proto field 13 (build_num), an internal build id
// that does not appear in the depot. tools/update_cs2_pins.py --demo <file.dem>
// refreshes it; otherwise it keeps the last value read from a real demo.
//
// Interface strings sourced from a2x/cs2-dumper (engine2.dll exports) and
// verified against the interface names CS2 asks for (game/csgo/bin/win64/client.dll):
// SteamClient023, SteamUser023,
// SteamGameCoordinator001, GameEventSystemClientV001, Source2EngineToClient001.
// CS2 renamed everything — Source 2 dropped the Source 1 naming convention:
//   GAMEEVENTSMANAGER002  ->  GameEventSystemClientV001
//   VEngineClient014      ->  Source2EngineToClient001
//
// NOTE: vtable layouts differ from Source 1. GetLocalPlayer/GetPlayerInfo
// offsets in IVEngineClient do NOT map to Source2EngineToClient001.
// The engine client hook is currently a no-op for CS2 (nullptr skips it).
// Game event registration (AddListener) also needs vtable research for CS2.
//
// Pinned: build 14193 | ClientVersion 2000939 | PatchVersion 1.41.9.4 | cs2-dumper 2026-10-30
// Run tools/update_cs2_pins.py after every CS2 patch; do not hand-edit the offsets.
const GameProfile g_profileCS2 = {
    .mode                    = GameMode::CS2,
    .appId                   = 730,
    .engineModule            = "engine2",
    .gameEventManagerVersion = "GameEventSystemClientV001",  // vtable-compatible with IGameEventManager2 (verified via hl2sdk-cs2)
    .engineClientVersion     = nullptr, // Source2EngineToClient001 vtable differs; use offset path instead
    .requiredAppIdVersion    = 14194,   // steam.inf PatchVersion, '.' stripped
    .requiredAppIdVersion2   = 10772,   // demo CDemoFileHeader build_num (June 2026 demo)
    .pricesheetVersion       = 0,
    .activeTournamentEventId = 0,
    // Source: a2x/cs2-dumper offsets.json (see the "Pinned:" line above)
    .localPlayerControllerOffset  = 0x25413F8, // dwLocalPlayerController (client.dll)
    .networkGameClientOffset      = 0x9B88C0,  // dwNetworkGameClient (engine2.dll)
    .networkGameClientLocalPlayer = 0xF8,      // offset within INetworkGameClient
};

// TF2 is Source 1, same vintage as CS:GO's engine build, so this profile
// reuses CS:GO's interface version strings as a best-effort default.
// UNVERIFIED against a real tf/bin engine.dylib/.so/.dll — if hooking fails
// on launch, dump the actual "GAMEEVENTSMANAGER*"/"VEngineClient*" strings
// out of TF2's engine binary and fix these. See docs/tf2_live_hook.md.
// requiredAppIdVersion/pricesheetVersion/activeTournamentEventId and the
// localPlayerController*/networkGameClient* offsets are CS:GO/CS2-only
// fields (matchmaking hello, CS2 memory offsets); unused by ClientGCTF2.
const GameProfile g_profileTF2 = {
    .mode                    = GameMode::TF2,
    .appId                   = 440,
    .engineModule            = "engine",
    .gameEventManagerVersion = "GAMEEVENTSMANAGER002",
    .engineClientVersion     = "VEngineClient014",
    .requiredAppIdVersion    = 0,
    .requiredAppIdVersion2   = 0,
    .pricesheetVersion       = 0,
    .activeTournamentEventId = 0,
    .localPlayerControllerOffset = 0,
    .networkGameClientOffset     = 0,
    .networkGameClientLocalPlayer = 0,
};

const GameProfile &GetGameProfile()
{
    static const GameProfile *profile = nullptr;
    if (!profile)
    {
        // lazy init so GetConfig() is ready
        std::string_view game = GetConfig().Game();
        if (game == "cs2")
        {
            profile = &g_profileCS2;
        }
        else if (game == "tf2")
        {
            profile = &g_profileTF2;
        }
        else
        {
            profile = &g_profileCSGO;
        }

        Platform::Print("GameProfile: using %s profile\n",
            game == "cs2" ? "CS2" : (game == "tf2" ? "TF2" : "CS:GO"));
    }
    return *profile;
}
