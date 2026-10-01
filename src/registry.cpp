/* SPDX-License-Identifier: MIT */

#include "../include/rinmime/registry.hpp"
#include "../include/rinmime/registry.h"

namespace rinmime {

std::string normalizeMediaType(const std::string& mediaType) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
    try {
#endif
    std::string result(mediaType.size(), '\0');
    size_t resultSize = 0u;
    const char* input = mediaType.empty() ? nullptr : mediaType.data();
    char* output = result.empty() ? nullptr : result.data();
    if (!rin_mime_registry_normalize_media_type(
            input, mediaType.size(), output, result.size(), &resultSize))
        return {};
    result.resize(resultSize);
    return result;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
    } catch (const std::bad_alloc&) {
        return {};
    }
#endif
}

bool mediaTypesEqual(const std::string& left, const std::string& right) {
    const std::string normalizedLeft = normalizeMediaType(left);
    const std::string normalizedRight = normalizeMediaType(right);
    return !normalizedLeft.empty() && normalizedLeft == normalizedRight;
}

const char* mediaTypeForExtension(const std::string& extension) {
    return rin_mime_registry_media_type_for_extension_view(
        extension.empty() ? nullptr : extension.data(), extension.size());
}

const char* preferredExtensionForMediaType(const std::string& mediaType) {
    return rin_mime_registry_preferred_extension_view(
        mediaType.empty() ? nullptr : mediaType.data(), mediaType.size());
}

} // namespace rinmime
