#include "stdafx.h"
#include "config.h"
#include "inventory.h"
#include "test_filesystem.h"

#include <cstdarg>
#include <cstdio>
#include <iterator>

namespace
{

constexpr uint64_t TestSteamId = 76561198000000000ull;

const CSOEconItemAttribute *FindAttribute(const CSOEconItem &item, uint32_t defIndex)
{
    for (const CSOEconItemAttribute &attribute : item.attribute())
    {
        if (attribute.def_index() == defIndex)
        {
            return &attribute;
        }
    }

    return nullptr;
}

bool FloatNear(float left, float right)
{
    float delta = left - right;
    return delta > -0.0001f && delta < 0.0001f;
}

bool CheckBasicItem(const Inventory &inventory, uint64_t itemId)
{
    const CSOEconItem *item = inventory.GetItem(itemId);
    if (!item)
    {
        return false;
    }

    const ItemSchema &schema = inventory.GetItemSchema();

    bool success = true;
    success &= item->def_index() == 7;
    success &= item->quality() == ItemSchema::QualityUnique;
    success &= item->rarity() == ItemSchema::RarityRare;
    success &= item->level() == 3;
    success &= item->custom_name() == "rcon item";
    success &= item->origin() == ItemOriginTraded;

    const CSOEconItemAttribute *paint = FindAttribute(*item, ItemSchema::AttributeTexturePrefab);
    const CSOEconItemAttribute *seed = FindAttribute(*item, ItemSchema::AttributeTextureSeed);
    const CSOEconItemAttribute *wear = FindAttribute(*item, ItemSchema::AttributeTextureWear);

    success &= paint && schema.AttributeUint32(paint) == 1352;
    success &= seed && schema.AttributeUint32(seed) == 41;
    success &= wear && FloatNear(schema.AttributeFloat(wear), 0.05f);

    return success;
}

bool CheckParameterizedItem(const Inventory &inventory, uint64_t itemId)
{
    const CSOEconItem *item = inventory.GetItem(itemId);
    if (!item)
    {
        return false;
    }

    const ItemSchema &schema = inventory.GetItemSchema();

    bool success = true;
    success &= item->def_index() == ItemSchema::ItemMusicKit;
    success &= item->quality() == ItemSchema::QualityStrange;
    success &= item->origin() == ItemOriginTraded;

    const CSOEconItemAttribute *music = FindAttribute(*item, ItemSchema::AttributeMusicId);
    const CSOEconItemAttribute *killEater = FindAttribute(*item, ItemSchema::AttributeKillEater);
    const CSOEconItemAttribute *scoreType = FindAttribute(*item, ItemSchema::AttributeKillEaterScoreType);
    const CSOEconItemAttribute *sprayTint = FindAttribute(*item, ItemSchema::AttributeSprayTintId);
    const CSOEconItemAttribute *sprayRemaining = FindAttribute(*item, ItemSchema::AttributeSpraysRemaining);
    const CSOEconItemAttribute *tournamentEvent = FindAttribute(*item, ItemSchema::AttributeTournamentEventId);
    const CSOEconItemAttribute *tournamentStage = FindAttribute(*item, ItemSchema::AttributeTournamentEventStageId);
    const CSOEconItemAttribute *stickerId = FindAttribute(*item, ItemSchema::AttributeStickerId0);
    const CSOEconItemAttribute *stickerWear = FindAttribute(*item, ItemSchema::AttributeStickerWear0);
    const CSOEconItemAttribute *stickerScale = FindAttribute(*item, ItemSchema::AttributeStickerScale0);
    const CSOEconItemAttribute *stickerRotation = FindAttribute(*item, ItemSchema::AttributeStickerRotation0);

    success &= music && schema.AttributeUint32(music) == 3;
    success &= killEater && schema.AttributeUint32(killEater) == 5;
    success &= scoreType && schema.AttributeUint32(scoreType) == 1;
    success &= sprayTint && schema.AttributeUint32(sprayTint) == 7;
    success &= sprayRemaining && schema.AttributeUint32(sprayRemaining) == 11;
    success &= tournamentEvent && schema.AttributeUint32(tournamentEvent) == 42;
    success &= tournamentStage && schema.AttributeUint32(tournamentStage) == 2;
    success &= stickerId && schema.AttributeUint32(stickerId) == 99;
    success &= stickerWear && FloatNear(schema.AttributeFloat(stickerWear), 0.1f);
    success &= stickerScale && FloatNear(schema.AttributeFloat(stickerScale), 1.5f);
    success &= stickerRotation && FloatNear(schema.AttributeFloat(stickerRotation), 30.0f);

    return success;
}

bool RunTests()
{
    if (!TestFilesystem::MakeDirectory("../../csgo_gc"))
    {
        return false;
    }

    // a previous failed run may have left state behind
    TestFilesystem::RemoveFile("../../csgo_gc/inventory.txt");

    bool success = true;

    {
        Inventory inventory{ TestSteamId };

        // no items_game.txt in the test working directory: exercises the raw fallback
        success &= !inventory.GetItemSchema().HasItemDefinitions();
        success &= inventory.ItemCount() == 0;

        Inventory::ParameterizedItemOptions options;
        options.paint = 1352;
        options.seed = 41;
        options.wear = 0.05f;
        options.rarity = ItemSchema::RarityRare;
        options.quality = ItemSchema::QualityUnique;
        options.level = 3;
        options.customName = "rcon item";

        CMsgSOSingleObject update;
        std::string error;
        uint64_t itemId = inventory.CreateRconItem(7, options, update, error);
        success &= itemId != 0;
        success &= error.empty();
        success &= inventory.ItemCount() == 1;
        success &= CheckBasicItem(inventory, itemId);

        Inventory::ParameterizedItemOptions fullOptions;
        fullOptions.music = 3;
        fullOptions.statTrak = 5;
        fullOptions.sprayColor = 7;
        fullOptions.sprayRemaining = 11;
        fullOptions.tournament.eventId = 42;
        fullOptions.tournament.stageId = 2;
        fullOptions.sticker[0] = 99;
        fullOptions.stickerWear[0] = 0.1f;
        fullOptions.stickerScale[0] = 1.5f;
        fullOptions.stickerRotation[0] = 30.0f;

        CMsgSOSingleObject musicUpdate;
        uint64_t musicItemId = inventory.CreateRconItem(ItemSchema::ItemMusicKit, fullOptions, musicUpdate, error);
        success &= musicItemId != 0;
        success &= error.empty();
        success &= inventory.ItemCount() == 2;
        success &= CheckParameterizedItem(inventory, musicItemId);

        // validation still applies to values that do not depend on the schema
        CMsgSOSingleObject invalidUpdate;
        Inventory::ParameterizedItemOptions invalidOptions;
        invalidOptions.music = 3;
        uint64_t invalidId = inventory.CreateRconItem(7, invalidOptions, invalidUpdate, error);
        success &= invalidId == 0;
        success &= error == "music requires defindex 1314";

        invalidOptions = {};
        invalidOptions.sprayColor = 0;
        invalidId = inventory.CreateRconItem(ItemSchema::ItemSpray, invalidOptions, invalidUpdate, error);
        success &= invalidId == 0;
        success &= error == "invalid parameter spray_color";

        success &= inventory.Save();
    }

    // read it back from disk
    {
        Inventory reloaded{ TestSteamId };
        success &= reloaded.ItemCount() == 2;

        if (reloaded.ItemCount() == 2)
        {
            const ItemMap &items = reloaded.Items();
            auto first = items.begin();
            auto second = std::next(first);
            success &= CheckBasicItem(reloaded, first->first) || CheckBasicItem(reloaded, second->first);
            success &= CheckParameterizedItem(reloaded, first->first)
                || CheckParameterizedItem(reloaded, second->first);
        }
    }

    {
        Inventory inventory{ TestSteamId };
        CMsgSOSingleObject destroyed;
        auto it = inventory.Items().begin();
        success &= it != inventory.Items().end();
        if (it != inventory.Items().end())
        {
            success &= inventory.RemoveItem(it->first, destroyed);
            success &= inventory.ItemCount() == 1;
        }
    }

    return success;
}

} // namespace

namespace Platform
{

void Print(const char *, ...)
{
}

} // namespace Platform

int main()
{
    const bool success = RunTests();

    TestFilesystem::RemoveFile("../../csgo_gc/inventory.txt");
    TestFilesystem::RemoveDir("../../csgo_gc");
    return success ? 0 : 1;
}
