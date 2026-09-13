/* SPDX-License-Identifier: MIT */
#ifndef RINMIME_TRANSFER_H
#define RINMIME_TRANSFER_H

#include <stddef.h>
#include <stdint.h>

#define RIN_MIME_NESTING_DEPTH_MAX 16u
#define RIN_MIME_PART_COUNT_MAX 256u
#define RIN_MIME_ATTACHMENT_COUNT_MAX 64u
#define RIN_MIME_DECODED_PART_MAX (64u * 1024u * 1024u)
#define RIN_MIME_RAW_MAX (32u * 1024u * 1024u)
#define RIN_MIME_FILENAME_MAX 255u

enum RinMimePolicyResult {
    RIN_MIME_OK = 0,
    RIN_MIME_INVALID_ARGUMENT = -1,
    RIN_MIME_MALFORMED = -2,
    RIN_MIME_TOO_LARGE = -3,
    RIN_MIME_UNSUPPORTED = -4
};

#ifdef __cplusplus
extern "C" {
#endif

int rin_mime_decode_transfer(const char* encoding, size_t encoding_size,
                             const uint8_t* input, size_t input_size,
                             uint8_t* output, size_t output_capacity,
                             size_t* output_size);
int rin_mime_filename_valid(const char* name, size_t size);

#ifdef __cplusplus
}
#endif

#endif
