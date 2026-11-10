#include "stdafx.h"
#include "config.h"
#include "inventory.h"
#include "test_filesystem.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
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
    const CSOEconItemAttribute *keychainId = FindAttribute(*item, ItemSchema::AttributeKeychainSlot0Id);
    const CSOEconItemAttribute *keychainX = FindAttribute(*item, ItemSchema::AttributeKeychainSlot0OffsetX);

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
    success &= keychainId && schema.AttributeUint32(keychainId) == 42;
    success &= keychainX && FloatNear(schema.AttributeFloat(keychainX), 0.0f);

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

        // No items_game.txt in the test working directory exercises the raw
        // fallback. When one is supplied (see tools/extract_items_game.py) the
        // real CS2 schema has to load and resolve crate loot lists.
        const bool haveSchema = inventory.GetItemSchema().HasItemDefinitions();
        if (!haveSchema)
        {
            success &= !inventory.GetItemSchema().HasItemDefinitions();
        }
        else
        {
            Platform::Print("schema check: crate 4745 loot list = %p\n",
                static_cast<const void *>(inventory.GetItemSchema().GetCrateLootList(4745)));
            Platform::Print("schema check: crate 4001 loot list = %p\n",
                static_cast<const void *>(inventory.GetItemSchema().GetCrateLootList(4001)));

            // 4745 = "crate_sticker_pack_rmr2020_contenders" (supply crate series 313)
            success &= inventory.GetItemSchema().GetCrateLootList(4745) != nullptr;
        }
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
        fullOptions.keychain = 42;
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

        // Charms: the client applies a keychain by sending CMsgModifyItemAttribute jobs, one
        // attribute at a time. Inventory::ModifyItemAttribute is what persists them, and
        // without it the attach spins forever (nothing was ever stored or answered).
        if (haveSchema)
        {
            CMsgSOSingleObject keychainUpdate;

            success &= inventory.ModifyItemAttribute(
                itemId, ItemSchema::AttributeKeychainSlot0Id, 42, keychainUpdate);
            const CSOEconItemAttribute *keychainId = FindAttribute(
                inventory.Items().at(itemId), ItemSchema::AttributeKeychainSlot0Id);
            success &= keychainId
                && inventory.GetItemSchema().AttributeUint32(keychainId) == 42;

            // offsets are floats: the wire value is the raw IEEE float, not the integer
            success &= inventory.ModifyItemAttribute(itemId,
                ItemSchema::AttributeKeychainSlot0OffsetX,
                0x3F800000, // 1.0f
                keychainUpdate);
            const CSOEconItemAttribute *offsetX = FindAttribute(
                inventory.Items().at(itemId), ItemSchema::AttributeKeychainSlot0OffsetX);
            success &= offsetX
                && inventory.GetItemSchema().AttributeFloat(offsetX) == 1.0f;

            // a zero keychain id is how the client takes a charm off again
            success &= inventory.ModifyItemAttribute(
                itemId, ItemSchema::AttributeKeychainSlot0Id, 0, keychainUpdate);
            keychainId = FindAttribute(
                inventory.Items().at(itemId), ItemSchema::AttributeKeychainSlot0Id);
            success &= keychainId
                && inventory.GetItemSchema().AttributeUint32(keychainId) == 0;

            // items we do not own are rejected instead of inventing an entry
            success &= !inventory.ModifyItemAttribute(0xDEAD, 299, 1, keychainUpdate);
        }

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

void Print(const char *format, ...)
{
    if (!getenv("GC_TEST_VERBOSE"))
    {
        return;
    }

    va_list ap;
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
}

} // namespace Platform

int main()
{
    const bool success = RunTests();

    TestFilesystem::RemoveFile("../../csgo_gc/inventory.txt");
    TestFilesystem::RemoveDir("../../csgo_gc");
    return success ? 0 : 1;
}
