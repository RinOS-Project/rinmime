/* SPDX-License-Identifier: MIT */
#ifndef RINMIME_REGISTRY_HPP
#define RINMIME_REGISTRY_HPP

#include "../../../libcxx/string.h"

namespace rinmime {

/* Normalize and compare a bare media type (for example, image/png). Parameters
 * are intentionally rejected here; Content-Type parameters belong to Part. */
std::string normalizeMediaType(const std::string& mediaType);
bool mediaTypesEqual(const std::string& left, const std::string& right);

/* Return pointers to immutable registry literals. Unknown values return null. */
const char* mediaTypeForExtension(const std::string& extension);
const char* preferredExtensionForMediaType(const std::string& mediaType);

} // namespace rinmime

#endif
