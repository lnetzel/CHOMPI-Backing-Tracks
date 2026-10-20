#include "BackingSettings.h"

#include <cassert>
#include <cstdio>

int main()
{
    chompi::BackingSettings settings = {0xFFFFFFFEu, {0, 1, 16, 31, 32}};
    char encoded[chompi::kBackingSettingsMaxBytes + 1] = {};
    size_t encoded_length = 0;
    assert(chompi::EncodeBackingSettings(settings, encoded, sizeof(encoded),
                                         encoded_length));
    assert(strstr(encoded, "\"line_in\":1") != NULL);

    chompi::BackingSettings decoded = {};
    assert(chompi::DecodeBackingSettings(encoded, encoded_length, decoded));
    assert(decoded.generation == settings.generation);
    for(size_t i = 0; i < chompi::kBackingLevelCount; ++i)
        assert(decoded.levels[i] == settings.levels[i]);

    assert(chompi::BackingGenerationNewer(1, 0xFFFFFFFEu));
    assert(!chompi::BackingGenerationNewer(0xFFFFFFFEu, 1));
    assert(!chompi::EncodeBackingSettings({1, {33, 0, 0, 0, 0}}, encoded,
                                          sizeof(encoded), encoded_length));
    encoded[encoded_length - 4]
        = encoded[encoded_length - 4] == '0' ? '1' : '0';
    assert(!chompi::DecodeBackingSettings(encoded, encoded_length, decoded));
    assert(!chompi::DecodeBackingSettings(encoded, encoded_length - 2, decoded));

    const char duplicate_field[]
        = "{\"version\":1,\"version\":1,\"generation\":0,"
          "\"levels\":{\"track\":16,\"line_in\":16,\"click\":0,"
          "\"headphones\":16,\"main\":16},\"checksum\":\"00000000\"}";
    assert(!chompi::DecodeBackingSettings(duplicate_field,
                                          sizeof(duplicate_field) - 1,
                                          decoded));

    std::puts("backing_settings_test: all assertions passed");
    return 0;
}