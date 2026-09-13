/* SPDX-License-Identifier: MIT */

#include "../include/rinmime/registry.h"

#include <stddef.h>

typedef struct RinMimeRegistryEntry {
    const char* extension;
    const char* media_type;
} RinMimeRegistryEntry;

static const RinMimeRegistryEntry g_entries[] = {
    {"txt", "text/plain"}, {"md", "text/markdown"},
    {"log", "text/plain"}, {"ini", "text/plain"},
    {"conf", "text/plain"}, {"cfg", "text/plain"},
    {"csv", "text/csv"}, {"html", "text/html"},
    {"htm", "text/html"}, {"css", "text/css"},
    {"xml", "application/xml"}, {"json", "application/json"},
    {"js", "text/javascript"}, {"ts", "text/typescript"},
    {"c", "text/x-c"}, {"h", "text/x-c"},
    {"cpp", "text/x-c++"}, {"hpp", "text/x-c++"},
    {"py", "text/x-python"}, {"sh", "text/x-shellscript"},
    {"wasm", "application/wasm"}, {"pdf", "application/pdf"},
    {"eml", "message/rfc822"}, {"zip", "application/zip"},
    {"tar", "application/x-tar"}, {"gz", "application/gzip"},
    {"png", "image/png"}, {"jpg", "image/jpeg"},
    {"jpeg", "image/jpeg"}, {"gif", "image/gif"},
    {"webp", "image/webp"}, {"bmp", "image/bmp"},
    {"ico", "image/x-icon"}, {"svg", "image/svg+xml"},
    {"mp3", "audio/mpeg"}, {"flac", "audio/flac"},
    {"wav", "audio/wav"}, {"ogg", "audio/ogg"},
    {"opus", "audio/opus"}, {"aac", "audio/aac"},
    {"m4a", "audio/mp4"}, {"mp4", "video/mp4"},
    {"webm", "video/webm"},
    {"docx", "application/vnd.openxmlformats-officedocument.wordprocessingml.document"},
    {"xlsx", "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"},
    {"pptx", "application/vnd.openxmlformats-officedocument.presentationml.presentation"}
};

static int ascii_space(unsigned char value)
{
    return value == (unsigned char)' ' || value == (unsigned char)'\t' ||
           value == (unsigned char)'\r' || value == (unsigned char)'\n';
}

static char ascii_lower(char value)
{
    return value >= 'A' && value <= 'Z' ? (char)(value + ('a' - 'A')) : value;
}

static int token_char(unsigned char value)
{
    return (value >= (unsigned char)'a' && value <= (unsigned char)'z') ||
           (value >= (unsigned char)'A' && value <= (unsigned char)'Z') ||
           (value >= (unsigned char)'0' && value <= (unsigned char)'9') ||
           value == (unsigned char)'!' || value == (unsigned char)'#' ||
           value == (unsigned char)'$' || value == (unsigned char)'%' ||
           value == (unsigned char)'&' || value == (unsigned char)'\'' ||
           value == (unsigned char)'*' || value == (unsigned char)'+' ||
           value == (unsigned char)'-' || value == (unsigned char)'.' ||
           value == (unsigned char)'^' || value == (unsigned char)'_' ||
           value == (unsigned char)'`' || value == (unsigned char)'|' ||
           value == (unsigned char)'~';
}

static size_t literal_size(const char* value)
{
    size_t size = 0u;
    while (value[size] != '\0') ++size;
    return size;
}

static int literal_equal_ci(const char* input, size_t input_size,
                            const char* literal)
{
    size_t index;
    if (input == NULL || literal == NULL || input_size != literal_size(literal))
        return 0;
    for (index = 0u; index < input_size; ++index)
        if (ascii_lower(input[index]) != ascii_lower(literal[index])) return 0;
    return 1;
}

static int normalize_media_type(const char* input, size_t input_size,
                                char* output, size_t output_capacity,
                                size_t* output_size)
{
    size_t begin = 0u;
    size_t end = input_size;
    size_t slash = (size_t)-1;
    size_t index;
    if (output_size == NULL || (input_size != 0u && input == NULL) ||
        (output_capacity != 0u && output == NULL)) {
        if (output_size != NULL) *output_size = 0u;
        return 0;
    }
    *output_size = 0u;
    while (begin < end && ascii_space((unsigned char)input[begin])) ++begin;
    while (end > begin && ascii_space((unsigned char)input[end - 1u])) --end;
    if (begin == end || end - begin > RIN_MIME_REGISTRY_MAX_MEDIA_TYPE_BYTES)
        return 0;
    for (index = begin; index < end; ++index) {
        unsigned char value = (unsigned char)input[index];
        if (value == (unsigned char)'/') {
            if (slash != (size_t)-1) return 0;
            slash = index - begin;
        } else if (!token_char(value)) {
            return 0;
        }
    }
    if (slash == (size_t)-1 || slash == 0u || slash + 1u == end - begin ||
        end - begin > output_capacity)
        return 0;
    for (index = 0u; index < end - begin; ++index)
        output[index] = ascii_lower(input[begin + index]);
    *output_size = end - begin;
    return 1;
}

static int normalize_extension(const char* input, size_t input_size,
                               char* output, size_t output_capacity,
                               size_t* output_size)
{
    size_t begin = 0u;
    size_t index;
    if (output_size == NULL || (input_size != 0u && input == NULL) ||
        (output_capacity != 0u && output == NULL)) {
        if (output_size != NULL) *output_size = 0u;
        return 0;
    }
    *output_size = 0u;
    if (input_size != 0u && input[0] == '.') begin = 1u;
    if (begin == input_size || input_size - begin >
        RIN_MIME_REGISTRY_MAX_EXTENSION_BYTES ||
        input_size - begin > output_capacity)
        return 0;
    for (index = begin; index < input_size; ++index) {
        unsigned char value = (unsigned char)input[index];
        if (!token_char(value) || value == (unsigned char)'.') return 0;
        output[index - begin] = ascii_lower(input[index]);
    }
    *output_size = input_size - begin;
    return 1;
}

static const char* extension_lookup(const char* extension,
                                    size_t extension_size)
{
    char normalized[RIN_MIME_REGISTRY_MAX_EXTENSION_BYTES + 1u];
    size_t normalized_size = 0u;
    size_t index;
    if (!normalize_extension(extension, extension_size, normalized,
                             sizeof(normalized), &normalized_size))
        return NULL;
    for (index = 0u; index < sizeof(g_entries) / sizeof(g_entries[0]); ++index)
        if (literal_equal_ci(normalized, normalized_size,
                             g_entries[index].extension))
            return g_entries[index].media_type;
    return NULL;
}

static int copy_view(const char* value, char* output, size_t output_capacity,
                     size_t* output_size)
{
    size_t size;
    size_t index;
    if (output_size == NULL || (output_capacity != 0u && output == NULL)) {
        if (output_size != NULL) *output_size = 0u;
        return 0;
    }
    *output_size = 0u;
    if (value == NULL) return 0;
    size = literal_size(value);
    if (size > output_capacity) return 0;
    for (index = 0u; index < size; ++index) output[index] = value[index];
    *output_size = size;
    return 1;
}

int rin_mime_registry_normalize_media_type(
    const char* input, size_t input_size, char* output, size_t output_capacity,
    size_t* output_size)
{
    return normalize_media_type(input, input_size, output, output_capacity,
                                output_size);
}

const char* rin_mime_registry_media_type_for_extension_view(
    const char* extension, size_t extension_size)
{
    return extension_lookup(extension, extension_size);
}

const char* rin_mime_registry_media_type_for_path_view(
    const char* path, size_t path_size)
{
    size_t component_begin = 0u;
    size_t dot = (size_t)-1;
    size_t index;
    if (path == NULL || path_size == 0u) return NULL;
    for (index = 0u; index < path_size; ++index) {
        if (path[index] == '/' || path[index] == '\\') {
            component_begin = index + 1u;
            dot = (size_t)-1;
        } else if (path[index] == '.') {
            dot = index;
        }
    }
    if (dot == (size_t)-1 || dot < component_begin ||
        dot + 1u >= path_size)
        return NULL;
    return extension_lookup(path + dot + 1u, path_size - dot - 1u);
}

const char* rin_mime_registry_preferred_extension_view(
    const char* media_type, size_t media_type_size)
{
    char normalized[RIN_MIME_REGISTRY_MAX_MEDIA_TYPE_BYTES + 1u];
    size_t normalized_size = 0u;
    size_t index;
    if (!normalize_media_type(media_type, media_type_size, normalized,
                              sizeof(normalized), &normalized_size))
        return NULL;
    for (index = 0u; index < sizeof(g_entries) / sizeof(g_entries[0]); ++index)
        if (literal_equal_ci(normalized, normalized_size,
                             g_entries[index].media_type))
            return g_entries[index].extension;
    return NULL;
}

int rin_mime_registry_media_type_for_extension(
    const char* extension, size_t extension_size, char* output,
    size_t output_capacity, size_t* output_size)
{
    return copy_view(rin_mime_registry_media_type_for_extension_view(
                         extension, extension_size),
                     output, output_capacity, output_size);
}

int rin_mime_registry_media_type_for_path(
    const char* path, size_t path_size, char* output, size_t output_capacity,
    size_t* output_size)
{
    return copy_view(rin_mime_registry_media_type_for_path_view(path, path_size),
                     output, output_capacity, output_size);
}

int rin_mime_registry_preferred_extension(
    const char* media_type, size_t media_type_size, char* output,
    size_t output_capacity, size_t* output_size)
{
    return copy_view(rin_mime_registry_preferred_extension_view(
                         media_type, media_type_size),
                     output, output_capacity, output_size);
}
