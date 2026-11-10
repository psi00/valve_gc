#pragma once

#include "item_schema.h"

// Validation for the charm/sticker placement values RCON can write, shared by the `give_item`
// keys (`keychain_x/y/z`, `keychain_seed`, `sticker<N>_x/y/schema`) and by `set_item_attr`.
//
// The valid envelope of a charm or sticker is per weapon model and lives in the client's model
// data, not in items_game.txt, so the GC does not try to reproduce it: these checks are
// mechanical only (finite, 0.0001 grid, loose typo caps) and the client clamps to the real
// bounds. Kept out of gc_client.cpp so the rules are testable without a live GC.
namespace RconPlacement
{

// Loose absolute cap for an offset. Legitimate long-weapon coordinates reach ~41 units, so this
// only catches typos like a missing decimal point.
constexpr float OffsetLimit = 1000.0f;

// Dangle seed range from the shipped schema; it drives the charm pose, not the paint.
constexpr uint32_t KeychainSeedMin = 1;
constexpr uint32_t KeychainSeedMax = 100000;

// Sticker slots 0..5 in the schema.
constexpr uint32_t StickerSchemaMax = 5;

// The client stores charm and sticker placement on a 0.0001 grid.
float Round(float value);

// "%.4f": the same text is echoed in the RCON reply and parsed again for storage.
std::string Format(float value);

// A float attribute that holds a charm or sticker position (as opposed to e.g. wear).
bool IsPlacementAttribute(uint32_t attrDefIndex);

// 299..321, the slot 0 charm id and its companions. Only these answer the client with a
// keychain notification; sticker placement travels through the same message.
bool IsKeychainAttribute(uint32_t attrDefIndex);

// 290..295, the physical anchor ("schema") index of a sticker slot.
bool IsStickerSchemaAttribute(uint32_t attrDefIndex);

// Value parsers. A false result means the caller answers ERR invalid parameter <key>.
bool ParseOffset(std::string_view text, float &out);
bool ParseSeed(std::string_view text, uint32_t &out);
bool ParseStickerSchema(std::string_view text, uint32_t &out);

enum class AttributeValueStatus
{
    Ok,
    UnknownAttribute,
    InvalidValue
};

// Turns an RCON string into the value Inventory::SetItemAttribute should store, applying the
// attribute's schema type and the same rounding and caps the give_item keys use.
AttributeValueStatus CoerceAttributeValue(const ItemSchema &schema,
    uint32_t attrDefIndex,
    std::string_view value,
    std::string &out);

} // namespace RconPlacement
