/* SPDX-License-Identifier: MIT */
#ifndef RINMIME_REGISTRY_H
#define RINMIME_REGISTRY_H

#include <stddef.h>

#define RIN_MIME_REGISTRY_MAX_EXTENSION_BYTES 31u
#define RIN_MIME_REGISTRY_MAX_MEDIA_TYPE_BYTES 127u

#ifdef __cplusplus
extern "C" {
#endif

/* Normalize one bare media type into a caller-owned buffer. Parameters are
 * intentionally rejected; callers that need them must retain the MIME Part
 * parser contract. Returns non-zero on success. */
int rin_mime_registry_normalize_media_type(const char* input, size_t input_size,
                                           char* output, size_t output_capacity,
                                           size_t* output_size);

/* Return immutable registry views. A view is null when the extension/path or
 * media type is unknown or malformed. The input is a counted byte string;
 * path classification examines only the final component and never probes the
 * filesystem. */
const char* rin_mime_registry_media_type_for_extension_view(
    const char* extension, size_t extension_size);
const char* rin_mime_registry_media_type_for_path_view(
    const char* path, size_t path_size);
const char* rin_mime_registry_preferred_extension_view(
    const char* media_type, size_t media_type_size);

/* Copy a known registry value to caller-owned storage. The result does not
 * include a terminating NUL; output_size reports the exact byte count. */
int rin_mime_registry_media_type_for_extension(
    const char* extension, size_t extension_size, char* output,
    size_t output_capacity, size_t* output_size);
int rin_mime_registry_media_type_for_path(
    const char* path, size_t path_size, char* output, size_t output_capacity,
    size_t* output_size);
int rin_mime_registry_preferred_extension(
    const char* media_type, size_t media_type_size, char* output,
    size_t output_capacity, size_t* output_size);

#ifdef __cplusplus
}
#endif

#endif /* RINMIME_REGISTRY_H */
