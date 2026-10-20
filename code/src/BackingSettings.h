#pragma once

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "core_json.h"

namespace chompi
{

constexpr size_t kBackingLevelCount = 5;
constexpr size_t kBackingSettingsMaxBytes = 256;

struct BackingSettings
{
    uint32_t generation;
    uint8_t levels[kBackingLevelCount];
};

inline uint32_t BackingSettingsCrc32(const uint8_t *data, size_t length)
{
    uint32_t crc = 0xFFFFFFFFu;
    for(size_t i = 0; i < length; ++i)
    {
        crc ^= data[i];
        for(int bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

namespace backing_settings_detail
{
inline bool KeyIs(const JSONPair_t &pair, const char *key)
{
    const size_t key_length = strlen(key);
    return pair.key != NULL && pair.keyLength == key_length
           && memcmp(pair.key, key, key_length) == 0;
}

inline bool ReadInteger(const JSONPair_t &pair,
                        uint32_t maximum,
                        uint32_t &value)
{
    if(pair.jsonType != JSONNumber || pair.valueLength == 0)
        return false;
    uint32_t parsed = 0;
    for(size_t i = 0; i < pair.valueLength; ++i)
    {
        const char digit = pair.value[i];
        if(digit < '0' || digit > '9')
            return false;
        const uint32_t numeric_digit = static_cast<uint32_t>(digit - '0');
        if(numeric_digit > maximum
           || parsed > (maximum - numeric_digit) / 10u)
            return false;
        parsed = parsed * 10u + numeric_digit;
    }
    value = parsed;
    return true;
}

inline uint32_t Checksum(const BackingSettings &settings)
{
    uint8_t payload[1 + sizeof(settings.generation) + kBackingLevelCount] = {};
    payload[0] = 1;
    payload[1] = static_cast<uint8_t>(settings.generation);
    payload[2] = static_cast<uint8_t>(settings.generation >> 8);
    payload[3] = static_cast<uint8_t>(settings.generation >> 16);
    payload[4] = static_cast<uint8_t>(settings.generation >> 24);
    for(size_t i = 0; i < kBackingLevelCount; ++i)
        payload[5 + i] = settings.levels[i];
    return BackingSettingsCrc32(payload, sizeof(payload));
}

inline bool ParseLevels(const char *json,
                        size_t length,
                        const JSONPair_t &object,
                        BackingSettings &settings)
{
    if(object.jsonType != JSONObject)
        return false;
    const size_t offset = static_cast<size_t>(object.value - json);
    size_t start = offset;
    size_t next = offset;
    JSONPair_t field = {};
    unsigned seen = 0;
    JSONStatus_t status
        = JSON_Iterate(json, length, &start, &next, &field);
    while(status == JSONSuccess)
    {
        size_t level_index = 0;
        if(KeyIs(field, "track"))
            level_index = 0;
        else if(KeyIs(field, "line_in"))
            level_index = 1;
        else if(KeyIs(field, "click"))
            level_index = 2;
        else if(KeyIs(field, "headphones"))
            level_index = 3;
        else if(KeyIs(field, "main"))
            level_index = 4;
        else
            return false;
        const unsigned bit = 1u << level_index;
        uint32_t level = 0;
        if((seen & bit) != 0 || !ReadInteger(field, 32, level))
            return false;
        settings.levels[level_index] = static_cast<uint8_t>(level);
        seen |= bit;
        status = JSON_Iterate(json, length, &start, &next, &field);
    }
    return status == JSONNotFound && seen == 0x1fu;
}

inline bool ParseChecksum(const JSONPair_t &pair, uint32_t &checksum)
{
    if(pair.jsonType != JSONString || pair.valueLength != 8)
        return false;
    uint32_t parsed = 0;
    for(size_t i = 0; i < pair.valueLength; ++i)
    {
        const char digit = pair.value[i];
        uint32_t nibble = 0;
        if(digit >= '0' && digit <= '9')
            nibble = static_cast<uint32_t>(digit - '0');
        else if(digit >= 'A' && digit <= 'F')
            nibble = static_cast<uint32_t>(digit - 'A' + 10);
        else if(digit >= 'a' && digit <= 'f')
            nibble = static_cast<uint32_t>(digit - 'a' + 10);
        else
            return false;
        parsed = (parsed << 4) | nibble;
    }
    checksum = parsed;
    return true;
}

} // namespace backing_settings_detail

inline bool EncodeBackingSettings(const BackingSettings &settings,
                                  char *output,
                                  size_t output_size,
                                  size_t &output_length)
{
    if(output == NULL || output_size == 0)
        return false;
    for(size_t i = 0; i < kBackingLevelCount; ++i)
        if(settings.levels[i] > 32)
            return false;
    const int length = snprintf(
        output, output_size,
        "{\"version\":1,\"generation\":%lu,\"levels\":{\"track\":%u,\"line_in\":%u,\"click\":%u,\"headphones\":%u,\"main\":%u},\"checksum\":\"%08lX\"}\n",
        static_cast<unsigned long>(settings.generation),
        static_cast<unsigned>(settings.levels[0]),
        static_cast<unsigned>(settings.levels[1]),
        static_cast<unsigned>(settings.levels[2]),
        static_cast<unsigned>(settings.levels[3]),
        static_cast<unsigned>(settings.levels[4]),
        static_cast<unsigned long>(
            backing_settings_detail::Checksum(settings)));
    if(length <= 0 || static_cast<size_t>(length) >= output_size
       || static_cast<size_t>(length) > kBackingSettingsMaxBytes)
        return false;
    output_length = static_cast<size_t>(length);
    return true;
}

inline bool DecodeBackingSettings(const char *json,
                                  size_t length,
                                  BackingSettings &settings)
{
    if(json == NULL || length == 0 || length > kBackingSettingsMaxBytes
       || JSON_Validate(json, length) != JSONSuccess)
        return false;
    size_t root_offset = 0;
    while(root_offset < length
          && (json[root_offset] == ' ' || json[root_offset] == '\t'
              || json[root_offset] == '\r' || json[root_offset] == '\n'))
        ++root_offset;
    if(root_offset >= length || json[root_offset] != '{')
        return false;

    BackingSettings parsed = {};
    uint32_t version = 0;
    uint32_t expected_checksum = 0;
    size_t start = root_offset;
    size_t next = root_offset;
    JSONPair_t field = {};
    unsigned seen = 0;
    JSONStatus_t status
        = JSON_Iterate(json, length, &start, &next, &field);
    while(status == JSONSuccess)
    {
        unsigned bit = 0;
        if(backing_settings_detail::KeyIs(field, "version"))
        {
            bit = 1u << 0;
            if(!backing_settings_detail::ReadInteger(field, 1, version)
               || version != 1)
                return false;
        }
        else if(backing_settings_detail::KeyIs(field, "generation"))
        {
            bit = 1u << 1;
            if(!backing_settings_detail::ReadInteger(
                   field, UINT32_MAX, parsed.generation))
                return false;
        }
        else if(backing_settings_detail::KeyIs(field, "levels"))
        {
            bit = 1u << 2;
            if(!backing_settings_detail::ParseLevels(
                   json, length, field, parsed))
                return false;
        }
        else if(backing_settings_detail::KeyIs(field, "checksum"))
        {
            bit = 1u << 3;
            if(!backing_settings_detail::ParseChecksum(
                   field, expected_checksum))
                return false;
        }
        else
            return false;
        if((seen & bit) != 0)
            return false;
        seen |= bit;
        status = JSON_Iterate(json, length, &start, &next, &field);
    }
    if(status != JSONNotFound || seen != 0x0fu
       || backing_settings_detail::Checksum(parsed) != expected_checksum)
        return false;
    settings = parsed;
    return true;
}

inline bool BackingGenerationNewer(uint32_t left, uint32_t right)
{
    return static_cast<int32_t>(left - right) > 0;
}

} // namespace chompi