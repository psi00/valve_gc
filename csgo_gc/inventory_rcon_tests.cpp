#include "stdafx.h"
#include "config.h"
#include "inventory.h"
#include "rcon_placement.h"
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
    const CSOEconItemAttribute *keychainY = FindAttribute(*item, ItemSchema::AttributeKeychainSlot0OffsetY);
    const CSOEconItemAttribute *keychainZ = FindAttribute(*item, ItemSchema::AttributeKeychainSlot0OffsetZ);
    const CSOEconItemAttribute *keychainSeed = FindAttribute(*item, ItemSchema::AttributeKeychainSlot0Seed);
    const CSOEconItemAttribute *stickerOffsetX = FindAttribute(*item, ItemSchema::AttributeStickerOffsetX0);
    const CSOEconItemAttribute *stickerOffsetY = FindAttribute(*item, ItemSchema::AttributeStickerOffsetY0);
    const CSOEconItemAttribute *stickerSchema = FindAttribute(*item, ItemSchema::AttributeStickerSchema0);

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
    // charm positions are absolute model-space coordinates, not deltas
    success &= keychainX && FloatNear(schema.AttributeFloat(keychainX), 6.6343f);
    success &= keychainY && FloatNear(schema.AttributeFloat(keychainY), 0.7039f);
    success &= keychainZ && FloatNear(schema.AttributeFloat(keychainZ), 2.475f);
    success &= keychainSeed && schema.AttributeUint32(keychainSeed) == 12345;
    // sticker placement: small deltas from the slot's physical anchor plus the anchor index
    success &= stickerOffsetX && FloatNear(schema.AttributeFloat(stickerOffsetX), 0.02f);
    success &= stickerOffsetY && FloatNear(schema.AttributeFloat(stickerOffsetY), -0.01f);
    success &= stickerSchema && schema.AttributeUint32(stickerSchema) == 2;

    return success;
}

bool CheckAttributeTypeTable(const ItemSchema &schema)
{
    // Every id declared in ItemSchema::Attribute has to resolve to a storage type: without one
    // SetAttributeFloat/SetAttributeUint32 assert and store nothing, and the tests run without
    // items_game.txt so the hardcoded fallback table is what answers here.
    std::vector<uint32_t> ids = {
        ItemSchema::AttributeTexturePrefab,
        ItemSchema::AttributeTextureSeed,
        ItemSchema::AttributeTextureWear,
        ItemSchema::AttributeKillEater,
        ItemSchema::AttributeKillEaterScoreType,
        ItemSchema::AttributeCustomName,
        ItemSchema::AttributeMusicId,
        ItemSchema::AttributeQuestId,
        ItemSchema::AttributeSpraysRemaining,
        ItemSchema::AttributeSprayTintId,
        ItemSchema::AttributeKeychainSlot0Id,
        ItemSchema::AttributeKeychainSlot0OffsetX,
        ItemSchema::AttributeKeychainSlot0OffsetY,
        ItemSchema::AttributeKeychainSlot0OffsetZ,
        ItemSchema::AttributeKeychainSlot0Seed,
        ItemSchema::AttributeKeychainSlot0Highlight,
        ItemSchema::AttributeKeychainSlot0Sticker,
        ItemSchema::AttributeTournamentEventId,
        ItemSchema::AttributeTournamentEventStageId,
        ItemSchema::AttributeTournamentTeam0Id,
        ItemSchema::AttributeTournamentTeam1Id,
        ItemSchema::AttributeTournamentMvpAccountId,
        ItemSchema::AttributeCasketItemsCount,
        ItemSchema::AttributeCasketModificationDate,
        ItemSchema::AttributeCasketIdLow,
        ItemSchema::AttributeCasketIdHigh,
    };

    // 113..136: sticker id/wear/scale/rotation, four attributes per slot
    for (uint32_t id = ItemSchema::AttributeStickerId0; id <= ItemSchema::AttributeStickerRotation5; id++)
    {
        ids.push_back(id);
    }

    // 278..289: six slots of offset x/y deltas; 290..295: six slot anchor indexes
    for (uint32_t id = ItemSchema::AttributeStickerOffsetX0; id <= ItemSchema::AttributeStickerOffsetY0 + 11; id++)
    {
        ids.push_back(id);
    }

    for (uint32_t id = ItemSchema::AttributeStickerSchema0; id <= ItemSchema::AttributeStickerSchema0 + 5; id++)
    {
        ids.push_back(id);
    }

    bool success = true;
    for (uint32_t id : ids)
    {
        if (!schema.GetAttributeType(id))
        {
            fprintf(stderr, "attribute %u has no storage type\n", id);
            success = false;
        }
    }

    // unknown ids stay unknown instead of defaulting to a type (both are unused in the shipped
    // items_game.txt as well, so the check holds with and without the schema)
    success &= !schema.GetAttributeType(65535);
    success &= !schema.GetAttributeType(0xFFFF'FFF0u);
    success &= schema.IsFloatAttribute(ItemSchema::AttributeStickerOffsetX0);
    success &= !schema.IsFloatAttribute(ItemSchema::AttributeStickerSchema0);
    return success;
}

bool CheckPlacementValidation(const ItemSchema &schema)
{
    // give_item and set_item_attr share these rules (rcon_placement.cpp). The client owns the
    // per-weapon bounds, so all the GC rejects is nonsense: non-finite values, values off the
    // 0.0001 grid, out-of-range seeds, and a sticker anchor above the last slot.
    bool success = true;

    float offset = 0.0f;
    success &= RconPlacement::ParseOffset("6.63430", offset) && FloatNear(offset, 6.6343f);
    success &= RconPlacement::ParseOffset("-11.0706", offset) && FloatNear(offset, -11.0706f);
    success &= RconPlacement::ParseOffset("0.00006", offset) && FloatNear(offset, 0.0001f);
    success &= !RconPlacement::ParseOffset("nan", offset);
    success &= !RconPlacement::ParseOffset("inf", offset);
    success &= !RconPlacement::ParseOffset("", offset);
    success &= !RconPlacement::ParseOffset("1.5x", offset);
    success &= !RconPlacement::ParseOffset("5000", offset);

    uint32_t seed = 0;
    success &= RconPlacement::ParseSeed("1", seed) && seed == 1;
    success &= RconPlacement::ParseSeed("100000", seed) && seed == 100000;
    success &= !RconPlacement::ParseSeed("0", seed);
    success &= !RconPlacement::ParseSeed("100001", seed);
    success &= !RconPlacement::ParseSeed("-5", seed);

    uint32_t stickerSchema = 0;
    success &= RconPlacement::ParseStickerSchema("0", stickerSchema) && stickerSchema == 0;
    success &= RconPlacement::ParseStickerSchema("5", stickerSchema) && stickerSchema == 5;
    success &= !RconPlacement::ParseStickerSchema("6", stickerSchema);
    success &= !RconPlacement::ParseStickerSchema("-1", stickerSchema);

    // set_item_attr parses through the attribute's schema type, not as raw bits
    std::string coerced;
    success &= RconPlacement::CoerceAttributeValue(schema, 65535, "1", coerced)
        == RconPlacement::AttributeValueStatus::UnknownAttribute;

    success &= RconPlacement::CoerceAttributeValue(
        schema, ItemSchema::AttributeKeychainSlot0OffsetX, "7.5", coerced)
        == RconPlacement::AttributeValueStatus::Ok;
    success &= coerced == "7.5000";

    success &= RconPlacement::CoerceAttributeValue(
        schema, ItemSchema::AttributeStickerOffsetY0, "-0.01", coerced)
        == RconPlacement::AttributeValueStatus::Ok;
    success &= coerced == "-0.0100";

    success &= RconPlacement::CoerceAttributeValue(
        schema, ItemSchema::AttributeStickerSchema0, "2", coerced)
        == RconPlacement::AttributeValueStatus::Ok;
    success &= coerced == "2";

    success &= RconPlacement::CoerceAttributeValue(
        schema, ItemSchema::AttributeCustomName, "renamed", coerced)
        == RconPlacement::AttributeValueStatus::Ok;
    success &= coerced == "renamed";

    success &= RconPlacement::CoerceAttributeValue(
        schema, ItemSchema::AttributeKeychainSlot0OffsetX, "nan", coerced)
        == RconPlacement::AttributeValueStatus::InvalidValue;
    success &= RconPlacement::CoerceAttributeValue(
        schema, ItemSchema::AttributeKeychainSlot0OffsetX, "5000", coerced)
        == RconPlacement::AttributeValueStatus::InvalidValue;
    success &= RconPlacement::CoerceAttributeValue(
        schema, ItemSchema::AttributeKeychainSlot0Seed, "0", coerced)
        == RconPlacement::AttributeValueStatus::InvalidValue;
    success &= RconPlacement::CoerceAttributeValue(
        schema, ItemSchema::AttributeStickerSchema0, "9", coerced)
        == RconPlacement::AttributeValueStatus::InvalidValue;

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
        success &= CheckAttributeTypeTable(inventory.GetItemSchema());
        success &= CheckPlacementValidation(inventory.GetItemSchema());

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
        fullOptions.keychainOffsetX = 6.6343f;
        fullOptions.keychainOffsetY = 0.7039f;
        fullOptions.keychainOffsetZ = 2.475f;
        fullOptions.keychainSeed = 12345;
        fullOptions.sprayColor = 7;
        fullOptions.sprayRemaining = 11;
        fullOptions.tournament.eventId = 42;
        fullOptions.tournament.stageId = 2;
        fullOptions.sticker[0] = 99;
        fullOptions.stickerWear[0] = 0.1f;
        fullOptions.stickerScale[0] = 1.5f;
        fullOptions.stickerRotation[0] = 30.0f;
        fullOptions.stickerOffsetX[0] = 0.02f;
        fullOptions.stickerOffsetY[0] = -0.01f;
        fullOptions.stickerSchema[0] = 2;

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

        // SetItemAttribute is the RCON-side counterpart of ModifyItemAttribute: it parses the
        // value as a string according to the schema type and persists the result, which is how
        // gc-editor repositions a charm on an item that already exists.
        {
            CMsgSOSingleObject attributeUpdate;
            success &= inventory.SetItemAttribute(itemId,
                ItemSchema::AttributeKeychainSlot0OffsetX, "7.5", attributeUpdate);
            const CSOEconItemAttribute *editedX = FindAttribute(
                inventory.Items().at(itemId), ItemSchema::AttributeKeychainSlot0OffsetX);
            success &= editedX
                && FloatNear(inventory.GetItemSchema().AttributeFloat(editedX), 7.5f);

            // floats are rounded onto the 0.0001 grid
            success &= inventory.SetItemAttribute(itemId,
                ItemSchema::AttributeKeychainSlot0OffsetY, "0.12345678", attributeUpdate);
            const CSOEconItemAttribute *editedY = FindAttribute(
                inventory.Items().at(itemId), ItemSchema::AttributeKeychainSlot0OffsetY);
            success &= editedY
                && FloatNear(inventory.GetItemSchema().AttributeFloat(editedY), 0.1235f);

            // unknown items and unknown attribute ids are rejected, not invented
            success &= !inventory.SetItemAttribute(0xDEAD,
                ItemSchema::AttributeKeychainSlot0OffsetX, "1.5", attributeUpdate);
            success &= !inventory.SetItemAttribute(itemId, 65535, "1", attributeUpdate);
            success &= FindAttribute(inventory.Items().at(itemId), 65535) == nullptr;

            // garbage is rejected and leaves the stored value alone
            success &= !inventory.SetItemAttribute(itemId,
                ItemSchema::AttributeKeychainSlot0OffsetX, "nan", attributeUpdate);
            success &= !inventory.SetItemAttribute(itemId,
                ItemSchema::AttributeKeychainSlot0OffsetX, "1.5x", attributeUpdate);
            success &= !inventory.SetItemAttribute(itemId,
                ItemSchema::AttributeKeychainSlot0OffsetX, "", attributeUpdate);
            editedX = FindAttribute(inventory.Items().at(itemId), ItemSchema::AttributeKeychainSlot0OffsetX);
            success &= editedX
                && FloatNear(inventory.GetItemSchema().AttributeFloat(editedX), 7.5f);
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

            // an RCON edit has to survive save/reload, or it vanishes on GC shutdown
            bool sawEditedOffset = false;
            for (const auto &pair : items)
            {
                const CSOEconItemAttribute *editedX = FindAttribute(
                    pair.second, ItemSchema::AttributeKeychainSlot0OffsetX);
                if (editedX
                    && FloatNear(reloaded.GetItemSchema().AttributeFloat(editedX), 7.5f))
                {
                    sawEditedOffset = true;
                }
            }
            success &= sawEditedOffset;
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
