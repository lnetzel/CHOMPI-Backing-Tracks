#pragma once

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "core_json.h"
#include "BackingTypes.h"

namespace chompi
{

namespace backing_set_detail
{

inline size_t CollectionOffset(const char *json, size_t length)
{
    size_t offset = 0;
    while(offset < length
          && (json[offset] == ' ' || json[offset] == '\t'
              || json[offset] == '\r' || json[offset] == '\n'))
        ++offset;
    return offset;
}

inline bool KeyIs(const JSONPair_t &pair, const char *key)
{
    const size_t length = strlen(key);
    return pair.key != NULL && pair.keyLength == length
           && memcmp(pair.key, key, length) == 0;
}

inline bool ReadNumber(const JSONPair_t &pair, double &number)
{
    if(pair.jsonType != JSONNumber || pair.valueLength == 0
       || pair.valueLength >= 64)
        return false;
    char text[64];
    memcpy(text, pair.value, pair.valueLength);
    text[pair.valueLength] = '\0';
    char *end = NULL;
    number = strtod(text, &end);
    return end == text + pair.valueLength && isfinite(number);
}

inline bool ReadInteger(const JSONPair_t &pair,
                        uint32_t minimum,
                        uint32_t maximum,
                        uint32_t &value)
{
    double number;
    if(!ReadNumber(pair, number) || number < minimum || number > maximum
       || floor(number) != number)
        return false;
    value = static_cast<uint32_t>(number);
    return true;
}

inline bool ReadPath(const JSONPair_t &pair, char *destination)
{
    static const char track_directory[] = "TRACKS/";
    const size_t track_directory_length = sizeof(track_directory) - 1;
    if(pair.jsonType != JSONString || pair.valueLength == 0
       || pair.valueLength > kBackingMaxPath
       || pair.valueLength <= track_directory_length
       || pair.value[0] == '/'
       || memcmp(pair.value, track_directory, track_directory_length) != 0)
        return false;
    size_t segment_start = 0;
    for(size_t i = 0; i <= pair.valueLength; ++i)
    {
        if(i < pair.valueLength)
        {
            const unsigned char ch = pair.value[i];
            if(ch < 0x20 || ch > 0x7e || ch == '\\' || ch == ':')
                return false;
        }
        if(i == pair.valueLength || pair.value[i] == '/')
        {
            const size_t segment_length = i - segment_start;
            if(segment_length == 0
               || (segment_length == 1 && pair.value[segment_start] == '.')
               || (segment_length == 2
                   && pair.value[segment_start] == '.'
                   && pair.value[segment_start + 1] == '.'))
                return false;
            segment_start = i + 1;
        }
    }
    memcpy(destination, pair.value, pair.valueLength);
    destination[pair.valueLength] = '\0';
    return true;
}

inline bool ParseMeter(const char *json,
                       size_t length,
                       const JSONPair_t &pair,
                       uint8_t &numerator,
                       uint8_t &denominator)
{
    if(pair.jsonType != JSONArray)
        return false;
    const size_t offset = static_cast<size_t>(pair.value - json);
    size_t start = offset;
    size_t next = offset;
    JSONPair_t item = {};
    uint32_t values[2] = {};
    size_t count = 0;
    JSONStatus_t status = JSON_Iterate(json, length, &start, &next, &item);
    while(status == JSONSuccess)
    {
        if(item.key != NULL || count >= 2
           || !ReadInteger(item, 1, 12, values[count]))
            return false;
        ++count;
        status = JSON_Iterate(json, length, &start, &next, &item);
    }
    if(status != JSONNotFound || count != 2)
        return false;
    if((values[1] != 4 && values[1] != 8)
       || values[0] < 2 || values[0] > 12
       || (values[1] == 8 && values[0] < 3))
        return false;
    numerator = static_cast<uint8_t>(values[0]);
    denominator = static_cast<uint8_t>(values[1]);
    return true;
}

inline bool ParseSong(const char *json,
                      size_t length,
                      const JSONPair_t &object,
                      BackingSong &song)
{
    if(object.jsonType != JSONObject)
        return false;
    song.midi_clock = true;
    const size_t offset = static_cast<size_t>(object.value - json);
    size_t start = offset;
    size_t next = offset;
    JSONPair_t field = {};
    unsigned seen = 0;
    JSONStatus_t status = JSON_Iterate(json, length, &start, &next, &field);
    while(status == JSONSuccess)
    {
        unsigned bit = 0;
        if(KeyIs(field, "file"))
        {
            bit = 1u << 0;
            if(!ReadPath(field, song.file))
                return false;
        }
        else if(KeyIs(field, "bpm"))
        {
            bit = 1u << 1;
            double bpm;
            if(!ReadNumber(field, bpm) || bpm < 30.0 || bpm > 300.0)
                return false;
            song.bpm = static_cast<float>(bpm);
        }
        else if(KeyIs(field, "meter"))
        {
            bit = 1u << 2;
            if(!ParseMeter(json, length, field, song.meter_numerator,
                           song.meter_denominator))
                return false;
        }
        else if(KeyIs(field, "first_downbeat_ms"))
        {
            bit = 1u << 3;
            if(!ReadInteger(field, 0, UINT32_MAX, song.first_downbeat_ms))
                return false;
        }
        else if(KeyIs(field, "advance"))
        {
            bit = 1u << 4;
            if(field.jsonType != JSONString || field.valueLength != 4)
                return false;
            if(memcmp(field.value, "auto", 4) == 0)
                song.advance = BackingAdvance::Auto;
            else if(memcmp(field.value, "wait", 4) == 0)
                song.advance = BackingAdvance::Wait;
            else if(memcmp(field.value, "loop", 4) == 0)
                song.advance = BackingAdvance::Loop;
            else
                return false;
        }
        else if(KeyIs(field, "midi_clock"))
        {
            bit = 1u << 5;
            if(field.jsonType != JSONTrue && field.jsonType != JSONFalse)
                return false;
            song.midi_clock = field.jsonType == JSONTrue;
        }
        else
            return false;
        if((seen & bit) != 0)
            return false;
        seen |= bit;
        status = JSON_Iterate(json, length, &start, &next, &field);
    }
    return status == JSONNotFound && (seen & 0x1fu) == 0x1fu;
}

} // namespace backing_set_detail

inline bool ParseBackingSet(const char *json, size_t length, BackingSet &set)
{
    if(json == NULL || length == 0 || length > kBackingMaxSetBytes
       || JSON_Validate(json, length) != JSONSuccess)
        return false;
    const size_t root_offset = backing_set_detail::CollectionOffset(json, length);
    if(root_offset >= length || json[root_offset] != '{')
        return false;

    BackingSet parsed = {};
    size_t start = root_offset;
    size_t next = root_offset;
    JSONPair_t field = {};
    unsigned seen = 0;
    JSONStatus_t status = JSON_Iterate(json, length, &start, &next, &field);
    while(status == JSONSuccess)
    {
        unsigned bit = 0;
        if(backing_set_detail::KeyIs(field, "version"))
        {
            bit = 1u << 0;
            uint32_t version;
            if(!backing_set_detail::ReadInteger(field, 1, 1, version))
                return false;
        }
        else if(backing_set_detail::KeyIs(field, "songs"))
        {
            bit = 1u << 1;
            if(field.jsonType != JSONArray)
                return false;
            const size_t array_offset
                = static_cast<size_t>(field.value - json);
            size_t array_start = array_offset;
            size_t array_next = array_offset;
            JSONPair_t entry = {};
            JSONStatus_t array_status = JSON_Iterate(
                json, length, &array_start, &array_next, &entry);
            while(array_status == JSONSuccess)
            {
                if(parsed.count >= kBackingMaxSongs
                   || !backing_set_detail::ParseSong(
                       json, length, entry, parsed.songs[parsed.count]))
                    return false;
                ++parsed.count;
                array_status = JSON_Iterate(json, length, &array_start,
                                             &array_next, &entry);
            }
            if(array_status != JSONNotFound || parsed.count == 0)
                return false;
        }
        else
            return false;
        if((seen & bit) != 0)
            return false;
        seen |= bit;
        status = JSON_Iterate(json, length, &start, &next, &field);
    }
    if(status != JSONNotFound || seen != 0x3u)
        return false;
    set = parsed;
    return true;
}

inline bool RewriteBackingAdvance(char *json,
                                  size_t length,
                                  size_t song_index,
                                  BackingAdvance advance)
{
    const char *replacement = BackingAdvanceName(advance);
    if(json == NULL || length == 0 || replacement == NULL
       || JSON_Validate(json, length) != JSONSuccess)
        return false;

    size_t root_start = backing_set_detail::CollectionOffset(json, length);
    size_t root_next = root_start;
    JSONPair_t field = {};
    JSONStatus_t status
        = JSON_Iterate(json, length, &root_start, &root_next, &field);
    bool songs_found = false;
    size_t token_offset = 0;
    bool token_found = false;
    while(status == JSONSuccess)
    {
        if(backing_set_detail::KeyIs(field, "songs"))
        {
            if(songs_found || field.jsonType != JSONArray)
                return false;
            songs_found = true;
            size_t array_start = static_cast<size_t>(field.value - json);
            size_t array_next = array_start;
            JSONPair_t entry = {};
            JSONStatus_t array_status = JSON_Iterate(
                json, length, &array_start, &array_next, &entry);
            size_t index = 0;
            while(array_status == JSONSuccess)
            {
                if(index == song_index)
                {
                    if(entry.jsonType != JSONObject)
                        return false;
                    size_t object_start
                        = static_cast<size_t>(entry.value - json);
                    size_t object_next = object_start;
                    JSONPair_t song_field = {};
                    JSONStatus_t object_status = JSON_Iterate(
                        json, length, &object_start, &object_next, &song_field);
                    while(object_status == JSONSuccess)
                    {
                        if(backing_set_detail::KeyIs(song_field, "advance"))
                        {
                            if(token_found || song_field.jsonType != JSONString
                               || song_field.valueLength != 4)
                                return false;
                            token_offset
                                = static_cast<size_t>(song_field.value - json);
                            token_found = true;
                        }
                        object_status = JSON_Iterate(
                            json, length, &object_start, &object_next,
                            &song_field);
                    }
                    if(object_status != JSONNotFound || !token_found)
                        return false;
                    break;
                }
                ++index;
                array_status = JSON_Iterate(json, length, &array_start,
                                             &array_next, &entry);
            }
            if(array_status != JSONSuccess && array_status != JSONNotFound)
                return false;
        }
        status = JSON_Iterate(json, length, &root_start, &root_next, &field);
    }
    if(status != JSONNotFound || !songs_found || !token_found
       || token_offset > length || length - token_offset < 4)
        return false;

    char previous[4];
    memcpy(previous, json + token_offset, sizeof(previous));
    memcpy(json + token_offset, replacement, sizeof(previous));
    if(JSON_Validate(json, length) != JSONSuccess)
    {
        memcpy(json + token_offset, previous, sizeof(previous));
        return false;
    }
    return true;
}

} // namespace chompi