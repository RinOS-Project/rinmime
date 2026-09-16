/* SPDX-License-Identifier: MIT */

#include <stddef.h>

#include "../include/rinmime/registry.h"

static void fill(char* output, size_t capacity, unsigned char value) {
    size_t index;
    for (index = 0u; index < capacity; ++index)
        output[index] = (char)value;
}

static int is_zero(const char* output, size_t capacity) {
    size_t index;
    for (index = 0u; index < capacity; ++index)
        if (output[index] != '\0') return 0;
    return 1;
}

int main(void) {
    char output[32];
    size_t output_size = 99u;

    fill(output, sizeof(output), 0xA5u);
    if (rin_mime_registry_normalize_media_type(
            "text/plain; charset=utf-8", 25u, output, sizeof(output),
            &output_size) != 0 || output_size != 0u ||
        !is_zero(output, sizeof(output)))
        return 1;

    output_size = 99u;
    fill(output, sizeof(output), 0xA5u);
    if (rin_mime_registry_media_type_for_extension(
            "unknown", 7u, output, sizeof(output), &output_size) != 0 ||
        output_size != 0u || !is_zero(output, sizeof(output)))
        return 2;

    output_size = 99u;
    fill(output, sizeof(output), 0xA5u);
    if (rin_mime_registry_preferred_extension(
            "application/unknown", 19u, output, sizeof(output),
            &output_size) != 0 || output_size != 0u ||
        !is_zero(output, sizeof(output)))
        return 3;

    output_size = 99u;
    fill(output, sizeof(output), 0xA5u);
    if (rin_mime_registry_media_type_for_path(
            "/tmp/unknown", 12u, output, sizeof(output), &output_size) != 0 ||
        output_size != 0u || !is_zero(output, sizeof(output)))
        return 4;

    return 0;
}
