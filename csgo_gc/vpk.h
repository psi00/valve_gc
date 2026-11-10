#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// Minimal reader for Valve Pak (VPK) v1/v2 archives.
//
// CS2 ships its item schema as csgo/scripts/items/items_game.txt *inside*
// pak01_*.vpk instead of as a loose file on disk, which is why a stock install
// logs "items_game.txt not found" and case opening has no loot lists to draw
// from. This reader pulls that one file out so the schema can be loaded.

namespace Vpk
{

// Absolute path of the game directory (the "game/" folder that holds
// "csgo/" and "csgo_gc/"), with a trailing separator.  Empty on failure.
// Resolved from the current working directory, which is <game>/bin/<arch>/
// while the GC is loaded.
std::string GameDir();

// Absolute path of the VPK index (e.g. "<game>/csgo/pak01_dir.vpk"), or empty
// if the game directory cannot be found.
std::string FindIndex();

// Extracts a single entry (e.g. "scripts/items/items_game.txt") from the
// archive indexed by indexPath.  Returns false if the entry or archive is
// missing.
bool ExtractFile(const std::string &indexPath, std::string_view entryPath, std::vector<uint8_t> &out);

} // namespace Vpk
