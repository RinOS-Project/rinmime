/* SPDX-License-Identifier: MIT */

#include <stddef.h>
#include <string.h>

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
    char oversized_path[RIN_MIME_REGISTRY_MAX_PATH_BYTES + 1u];
    static const char embedded_nul_path[] = {
        '/', 't', 'm', 'p', '/', 'r', 'e', 'p', 'o', 's', 'i', 't', 'o', 'r',
        'y', '\0', '.', 'p', 'n', 'g'
    };

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

    memset(oversized_path, 'a', sizeof(oversized_path));
    oversized_path[0] = '/';
    oversized_path[RIN_MIME_REGISTRY_MAX_PATH_BYTES - 4u] = '.';
    oversized_path[RIN_MIME_REGISTRY_MAX_PATH_BYTES - 3u] = 'p';
    oversized_path[RIN_MIME_REGISTRY_MAX_PATH_BYTES - 2u] = 'n';
    oversized_path[RIN_MIME_REGISTRY_MAX_PATH_BYTES - 1u] = 'g';
    if (rin_mime_registry_media_type_for_path_view(
            oversized_path, RIN_MIME_REGISTRY_MAX_PATH_BYTES + 1u) != NULL)
        return 5;
    if (rin_mime_registry_media_type_for_path_view(
            embedded_nul_path, sizeof(embedded_nul_path)) != NULL)
        return 6;

    return 0;
}
