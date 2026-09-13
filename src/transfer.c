/* SPDX-License-Identifier: MIT */
#include "../include/rinmime/transfer.h"
#include "../../rinencoding/include/rinencoding/encoding.h"

#include <string.h>

static int ascii_equal(const char* value, size_t size, const char* expected)
{
    size_t index;
    size_t expected_size = 0u;
    while (expected[expected_size] != '\0') ++expected_size;
    if (value == NULL || size != expected_size) return 0;
    for (index = 0u; index < size; ++index) {
        unsigned char left = (unsigned char)value[index];
        unsigned char right = (unsigned char)expected[index];
        if (left >= 'A' && left <= 'Z') left = (unsigned char)(left + 'a' - 'A');
        if (right >= 'A' && right <= 'Z') right = (unsigned char)(right + 'a' - 'A');
        if (left != right) return 0;
    }
    return 1;
}

static int decode_identity(const uint8_t* input, size_t input_size,
                           uint8_t* output, size_t output_capacity,
                           size_t* output_size)
{
    if (input_size > output_capacity ||
        (input_size != 0u && (input == NULL || output == NULL)))
        return RIN_MIME_TOO_LARGE;
    if (input_size != 0u) memcpy(output, input, input_size);
    *output_size = input_size;
    return RIN_MIME_OK;
}

static int decode_base64(const uint8_t* input, size_t input_size,
                         uint8_t* output, size_t output_capacity,
                         size_t* output_size)
{
    const int status = rin_encoding_base64_decode(
        input, input_size, RIN_ENCODING_BASE64_STANDARD,
        RIN_ENCODING_BASE64_ALLOW_UNPADDED |
            RIN_ENCODING_BASE64_ALLOW_WHITESPACE,
        output, output_capacity, output_size);
    if (status == RIN_ENCODING_OK) return RIN_MIME_OK;
    if (status == RIN_ENCODING_BUFFER_TOO_SMALL) return RIN_MIME_TOO_LARGE;
    return RIN_MIME_MALFORMED;
}

static int decode_quoted_printable(const uint8_t* input, size_t input_size,
                                   uint8_t* output, size_t output_capacity,
                                   size_t* output_size)
{
    const int status = rin_encoding_quoted_printable_decode(
        input, input_size, output, output_capacity, output_size);
    if (status == RIN_ENCODING_OK) return RIN_MIME_OK;
    if (status == RIN_ENCODING_BUFFER_TOO_SMALL) return RIN_MIME_TOO_LARGE;
    if (status == RIN_ENCODING_INVALID_ARGUMENT)
        return RIN_MIME_INVALID_ARGUMENT;
    return RIN_MIME_MALFORMED;
}

int rin_mime_decode_transfer(const char* encoding, size_t encoding_size,
                             const uint8_t* input, size_t input_size,
                             uint8_t* output, size_t output_capacity,
                             size_t* output_size)
{
    int result;
    size_t used = 0u;
    if (output_size == NULL || encoding_size > 64u ||
        (input_size != 0u && input == NULL) ||
        (output_capacity != 0u && output == NULL)) {
        if (output_size != NULL) *output_size = 0u;
        if (output != NULL && output_capacity != 0u) memset(output, 0, output_capacity);
        return RIN_MIME_INVALID_ARGUMENT;
    }
    if (output != NULL && output_capacity != 0u) memset(output, 0, output_capacity);
    if (encoding_size == 0u || ascii_equal(encoding, encoding_size, "7bit") ||
        ascii_equal(encoding, encoding_size, "8bit") ||
        ascii_equal(encoding, encoding_size, "binary")) {
        result = decode_identity(input, input_size, output, output_capacity, &used);
    } else if (ascii_equal(encoding, encoding_size, "base64")) {
        result = decode_base64(input, input_size, output, output_capacity, &used);
    } else if (ascii_equal(encoding, encoding_size, "quoted-printable")) {
        result = decode_quoted_printable(input, input_size, output,
                                         output_capacity, &used);
    } else {
        result = RIN_MIME_UNSUPPORTED;
    }
    if (result != RIN_MIME_OK) {
        if (output != NULL && output_capacity != 0u) memset(output, 0, output_capacity);
        *output_size = 0u;
        return result;
    }
    *output_size = used;
    return RIN_MIME_OK;
}

static int utf8_valid(const unsigned char* value, size_t size)
{
    size_t index = 0u;
    while (index < size) {
        const unsigned char first = value[index++];
        uint32_t codepoint;
        size_t count;
        uint32_t minimum;
        size_t continuation;
        if (first < 0x80u) continue;
        if (first >= 0xc2u && first <= 0xdfu) {
            codepoint = first & 0x1fu; count = 2u; minimum = 0x80u;
        } else if (first >= 0xe0u && first <= 0xefu) {
            codepoint = first & 0x0fu; count = 3u; minimum = 0x800u;
        } else if (first >= 0xf0u && first <= 0xf4u) {
            codepoint = first & 0x07u; count = 4u; minimum = 0x10000u;
        } else return 0;
        if (count > size - index) return 0;
        for (continuation = 1u; continuation < count; ++continuation) {
            const unsigned char byte = value[index++];
            if ((byte & 0xc0u) != 0x80u) return 0;
            codepoint = (codepoint << 6u) | (byte & 0x3fu);
        }
        if (codepoint < minimum || codepoint > 0x10ffffu ||
            (codepoint >= 0xd800u && codepoint <= 0xdfffu)) return 0;
    }
    return 1;
}

int rin_mime_filename_valid(const char* name, size_t size)
{
    size_t index;
    if (name == NULL || size == 0u || size > RIN_MIME_FILENAME_MAX ||
        (size == 1u && name[0] == '.') ||
        (size == 2u && name[0] == '.' && name[1] == '.') ||
        name[0] == ' ' || name[size - 1u] == ' ' || name[size - 1u] == '.')
        return 0;
    for (index = 0u; index < size; ++index) {
        const unsigned char value = (unsigned char)name[index];
        if (value == 0u || value <= 0x1fu || value == 0x7fu ||
            value == '/' || value == '\\' || value == ':') return 0;
    }
    return utf8_valid((const unsigned char*)name, size);
}
