#include "stdafx.h"
#include "rcon_placement.h"

#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>

namespace
{

bool IsKeychainSeedAttribute(uint32_t attrDefIndex)
{
    return attrDefIndex == ItemSchema::AttributeKeychainSlot0Seed;
}

bool ParseUint32(std::string_view text, uint32_t &out)
{
    std::string input{ text };
    if (input.empty())
    {
        return false;
    }

    // strtoul accepts a leading minus and wraps it, so reject signs before converting.
    char *end = nullptr;
    errno = 0;
    unsigned long long parsed = std::strtoull(input.c_str(), &end, 10);
    if (errno == ERANGE || end != input.c_str() + input.size() || input[0] == '-'
        || parsed > std::numeric_limits<uint32_t>::max())
    {
        return false;
    }

    out = static_cast<uint32_t>(parsed);
    return true;
}

} // namespace

namespace RconPlacement
{

float Round(float value)
{
    return std::round(value * 10000.0f) / 10000.0f;
}

std::string Format(float value)
{
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%.4f", value);
    return buffer;
}

bool IsPlacementAttribute(uint32_t attrDefIndex)
{
    if (attrDefIndex >= ItemSchema::AttributeStickerOffsetX0
        && attrDefIndex <= ItemSchema::AttributeStickerOffsetX0 + 11)
    {
        return true;
    }

    return attrDefIndex >= ItemSchema::AttributeKeychainSlot0OffsetX
        && attrDefIndex <= ItemSchema::AttributeKeychainSlot0OffsetZ;
}

bool IsKeychainAttribute(uint32_t attrDefIndex)
{
    return attrDefIndex == ItemSchema::AttributeKeychainSlot0Id
        || (attrDefIndex >= ItemSchema::AttributeKeychainSlot0OffsetX
            && attrDefIndex <= ItemSchema::AttributeKeychainSlot0OffsetZ)
        || attrDefIndex == ItemSchema::AttributeKeychainSlot0Seed
        || attrDefIndex == ItemSchema::AttributeKeychainSlot0Highlight
        || attrDefIndex == ItemSchema::AttributeKeychainSlot0Sticker;
}

bool IsStickerSchemaAttribute(uint32_t attrDefIndex)
{
    return attrDefIndex >= ItemSchema::AttributeStickerSchema0
        && attrDefIndex <= ItemSchema::AttributeStickerSchema0 + 5;
}

bool ParseOffset(std::string_view text, float &out)
{
    std::string input{ text };
    char *end = nullptr;
    errno = 0;
    float parsed = std::strtof(input.c_str(), &end);
    if (input.empty() || errno == ERANGE || end != input.c_str() + input.size()
        || !std::isfinite(parsed) || std::fabs(parsed) > OffsetLimit)
    {
        return false;
    }

    out = Round(parsed);
    return true;
}

bool ParseSeed(std::string_view text, uint32_t &out)
{
    uint32_t parsed;
    if (!ParseUint32(text, parsed) || parsed < KeychainSeedMin || parsed > KeychainSeedMax)
    {
        return false;
    }

    out = parsed;
    return true;
}

bool ParseStickerSchema(std::string_view text, uint32_t &out)
{
    uint32_t parsed;
    if (!ParseUint32(text, parsed) || parsed > StickerSchemaMax)
    {
        return false;
    }

    out = parsed;
    return true;
}

AttributeValueStatus CoerceAttributeValue(const ItemSchema &schema,
    uint32_t attrDefIndex,
    std::string_view value,
    std::string &out)
{
    std::optional<AttributeType> type = schema.GetAttributeType(attrDefIndex);
    if (!type)
    {
        return AttributeValueStatus::UnknownAttribute;
    }

    if (*type == AttributeType::String)
    {
        out.assign(value.data(), value.size());
        return AttributeValueStatus::Ok;
    }

    if (*type == AttributeType::Float)
    {
        std::string input{ value };
        char *end = nullptr;
        errno = 0;
        float parsed = std::strtof(input.c_str(), &end);
        if (input.empty() || errno == ERANGE || end != input.c_str() + input.size()
            || !std::isfinite(parsed))
        {
            return AttributeValueStatus::InvalidValue;
        }

        if (IsPlacementAttribute(attrDefIndex) && std::fabs(parsed) > OffsetLimit)
        {
            return AttributeValueStatus::InvalidValue;
        }

        out = Format(Round(parsed));
        return AttributeValueStatus::Ok;
    }

    uint32_t parsed;
    if (!ParseUint32(value, parsed))
    {
        return AttributeValueStatus::InvalidValue;
    }

    if (IsKeychainSeedAttribute(attrDefIndex)
        && (parsed < KeychainSeedMin || parsed > KeychainSeedMax))
    {
        return AttributeValueStatus::InvalidValue;
    }

    if (IsStickerSchemaAttribute(attrDefIndex) && parsed > StickerSchemaMax)
    {
        return AttributeValueStatus::InvalidValue;
    }

    out = std::to_string(parsed);
    return AttributeValueStatus::Ok;
}

} // namespace RconPlacement
