/* SPDX-License-Identifier: MIT */
#include "../include/rinmime/mime.hpp"

namespace rinmime {
namespace {

size_t cStringLength(const char* value) {
    size_t length = 0u;
    if (value == nullptr) return 0u;
    while (value[length] != '\0') ++length;
    return length;
}

std::string decimal(unsigned value) {
    std::string result;
    do {
        result.insert(result.begin(), static_cast<char>('0' + value % 10u));
        value /= 10u;
    } while (value != 0u);
    return result;
}

char asciiLower(char value) {
    return value >= 'A' && value <= 'Z'
        ? static_cast<char>(value + ('a' - 'A')) : value;
}

bool asciiSpace(unsigned char value) {
    return value == ' ' || value == '\t' || value == '\r' || value == '\n';
}

std::string trim(const std::string& value) {
    size_t begin = 0u;
    size_t end = value.size();
    while (begin < end && asciiSpace(static_cast<unsigned char>(value[begin]))) ++begin;
    while (end > begin && asciiSpace(static_cast<unsigned char>(value[end - 1u]))) --end;
    return value.substr(begin, end - begin);
}

std::string lower(std::string value) {
    for (char& byte : value) byte = asciiLower(byte);
    return value;
}

bool contentTypeConsistent(const Headers& headers) {
    std::string first;
    bool found = false;
    for (const auto& item : headers.values) {
        if (item.name != "content-type") continue;
        const std::string value = lower(trim(item.value));
        if (!found) {
            first = value;
            found = true;
        } else if (value != first) {
            return false;
        }
    }
    return true;
}

bool startsWithInsensitive(const std::string& value, const char* prefix) {
    size_t count = cStringLength(prefix);
    if (value.size() < count) return false;
    for (size_t index = 0u; index < count; ++index)
        if (asciiLower(value[index]) != asciiLower(prefix[index])) return false;
    return true;
}

bool headerNameValid(const std::string& name) {
    if (name.empty()) return false;
    for (char byte : name) {
        const unsigned char value = static_cast<unsigned char>(byte);
        if (!((value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') ||
              (value >= '0' && value <= '9') || value == '!' || value == '#' ||
              value == '$' || value == '%' || value == '&' || value == '\'' ||
              value == '*' || value == '+' || value == '-' || value == '.' ||
              value == '^' || value == '_' || value == '`' || value == '|' ||
              value == '~')) return false;
    }
    return true;
}

bool headerValueValid(const std::string& value, size_t maxLineBytes) {
    if (value.size() > maxLineBytes) return false;
    for (char byte : value) {
        const unsigned char valueByte = static_cast<unsigned char>(byte);
        if (valueByte == 0u || valueByte == 0x7fu ||
            (valueByte < 0x20u && valueByte != '\t')) return false;
    }
    return true;
}

bool parseHeaderBlock(const std::string& input, Headers& result,
                      const Limits& limits) {
    size_t cursor = 0u;
    result.values.clear();
    if (input.size() > limits.maxHeaderBytes) return false;
    while (cursor < input.size()) {
        size_t end = input.find('\n', cursor);
        if (end == std::string::npos) end = input.size();
        if (end - cursor > limits.maxLineBytes) return false;
        std::string line = input.substr(cursor, end - cursor);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        cursor = end == input.size() ? end : end + 1u;
        if (line.empty()) return true;
        if ((line[0] == ' ' || line[0] == '\t') && !result.values.empty()) {
            std::string continuation = trim(line);
            if (!headerValueValid(continuation, limits.maxLineBytes)) return false;
            result.values.back().value += " " + continuation;
            if (!headerValueValid(result.values.back().value, limits.maxLineBytes)) return false;
            continue;
        }
        const size_t colon = line.find(':');
        if (colon == std::string::npos) return false;
        std::string name = trim(line.substr(0u, colon));
        std::string value = trim(line.substr(colon + 1u));
        if (!headerNameValid(name) || !headerValueValid(value, limits.maxLineBytes) ||
            result.values.size() >= limits.maxParts) return false;
        result.values.push_back({lower(name), value});
    }
    return true;
}

std::string parameter(const std::string& value, const char* wanted,
                      size_t maxParameters, bool* valid = nullptr) {
    if (valid != nullptr) *valid = true;
    const std::string name = lower(wanted);
    size_t cursor = 0u;
    size_t parameterCount = 0u;
    while ((cursor = value.find(';', cursor)) != std::string::npos) {
        if (parameterCount++ >= maxParameters) {
            if (valid != nullptr) *valid = false;
            return {};
        }
        ++cursor;
        while (cursor < value.size() && asciiSpace(static_cast<unsigned char>(value[cursor]))) ++cursor;
        const size_t equals = value.find('=', cursor);
        if (equals == std::string::npos) break;
        std::string key = lower(trim(value.substr(cursor, equals - cursor)));
        size_t end = value.find(';', equals + 1u);
        if (end == std::string::npos) end = value.size();
        std::string result = trim(value.substr(equals + 1u, end - equals - 1u));
        if (result.size() >= 2u && result.front() == '"' && result.back() == '"')
            result = result.substr(1u, result.size() - 2u);
        if (key == name || key == name + "*") return result;
        cursor = end;
    }
    return {};
}

bool findBoundary(const std::string& body, const std::string& marker,
                  size_t from, size_t& position, bool& closing) {
    size_t cursor = from;
    while ((cursor = body.find(marker, cursor)) != std::string::npos) {
        const size_t after = cursor + marker.size();
        const bool atLineStart = cursor == 0u || body[cursor - 1u] == '\n';
        const bool validSuffix = after == body.size() || body.compare(after, 2u, "--") == 0 ||
            body.compare(after, 2u, "\r\n") == 0 ||
            (after < body.size() && body[after] == '\n');
        if (atLineStart && validSuffix) {
            position = cursor;
            closing = body.compare(after, 2u, "--") == 0;
            return true;
        }
        ++cursor;
    }
    return false;
}

bool splitMime(const Headers& headers, const std::string& body,
               const std::string& prefix, size_t depth,
               std::vector<Part>& parts, const Limits& limits) {
    if (depth >= limits.maxNestingDepth || parts.size() >= limits.maxParts) return false;
    if (!contentTypeConsistent(headers)) return false;
    std::string contentType = headers.get("content-type");
    const bool typeWasImplicit = contentType.empty();
    if (typeWasImplicit) contentType = "text/plain";
    const size_t semicolon = contentType.find(';');
    std::string type = lower(trim(contentType.substr(0u, semicolon)));
    if (startsWithInsensitive(type, "multipart/")) {
        bool parametersValid = true;
        const std::string boundary = parameter(contentType, "boundary", limits.maxParameters,
                                               &parametersValid);
        if (!parametersValid || boundary.empty() || boundary.size() > limits.maxBoundaryBytes)
            return false;
        const std::string marker = "--" + boundary;
        size_t cursor = 0u;
        unsigned child = 1u;
        bool terminated = false;
        size_t position = 0u;
        bool closing = false;
        while (findBoundary(body, marker, cursor, position, closing)) {
            size_t after = position + marker.size();
            if (closing) { terminated = true; break; }
            if (body.compare(after, 2u, "\r\n") == 0) after += 2u;
            else if (after < body.size() && body[after] == '\n') ++after;
            size_t next = 0u;
            bool nextClosing = false;
            if (!findBoundary(body, marker, after, next, nextClosing)) return false;
            (void)nextClosing;
            std::string raw = body.substr(after, next - after);
            while (!raw.empty() && (raw.back() == '\r' || raw.back() == '\n')) raw.pop_back();
            size_t separator = raw.find("\r\n\r\n");
            size_t skip = 4u;
            if (separator == std::string::npos) { separator = raw.find("\n\n"); skip = 2u; }
            if (separator == std::string::npos) return false;
            Headers childHeaders;
            if (!parseHeaderBlock(raw.substr(0u, separator), childHeaders, limits)) return false;
            const std::string number = decimal(child++);
            const std::string childId = prefix.empty() ? number : prefix + "." + number;
            if (!splitMime(childHeaders, raw.substr(separator + skip), childId,
                           depth + 1u, parts, limits)) return false;
            cursor = next;
        }
        return terminated;
    }
    Part part;
    part.id = prefix.empty() ? "1" : prefix;
    part.type = type;
    part.typeWasImplicit = typeWasImplicit;
    part.disposition = lower(trim(headers.get("content-disposition")));
    bool dispositionParametersValid = true;
    part.fileName = parameter(headers.get("content-disposition"), "filename",
                              limits.maxParameters, &dispositionParametersValid);
    if (!dispositionParametersValid) return false;
    bool contentTypeParametersValid = true;
    if (part.fileName.empty())
        part.fileName = parameter(contentType, "name", limits.maxParameters,
                                  &contentTypeParametersValid);
    if (!contentTypeParametersValid) return false;
    bool charsetParametersValid = true;
    part.charset = parameter(contentType, "charset", limits.maxParameters,
                             &charsetParametersValid);
    if (!charsetParametersValid) return false;
    const bool isAttachment = startsWithInsensitive(part.disposition, "attachment") ||
                              !part.fileName.empty();
    if (isAttachment && part.fileName.empty()) part.fileName = "attachment";
    if (isAttachment && !rin_mime_filename_valid(part.fileName.data(), part.fileName.size()))
        part.fileName = "attachment";
    const std::string encoding = headers.get("content-transfer-encoding");
    if (body.size() > RIN_MIME_DECODED_PART_MAX) return false;
    part.body.assign(body.size(), 0u);
    size_t decodedSize = 0u;
    const int result = rin_mime_decode_transfer(
        encoding.data(), encoding.size(), reinterpret_cast<const uint8_t*>(body.data()), body.size(),
        part.body.empty() ? nullptr : part.body.data(), part.body.size(), &decodedSize);
    if (result != RIN_MIME_OK) return false;
    part.body.resize(decodedSize);
    if (isAttachment) {
        size_t attachmentCount = 0u;
        for (const auto& existing : parts) if (!existing.fileName.empty()) ++attachmentCount;
        if (attachmentCount >= limits.maxAttachments) return false;
    }
    parts.push_back(std::move(part));
    return true;
}

bool mailboxAtext(unsigned char value) {
    return (value >= 'A' && value <= 'Z') ||
           (value >= 'a' && value <= 'z') ||
           (value >= '0' && value <= '9') || value == '!' || value == '#' ||
           value == '$' || value == '%' || value == '&' || value == '\'' ||
           value == '*' || value == '+' || value == '-' || value == '/' ||
           value == '=' || value == '?' || value == '^' || value == '_' ||
           value == '`' || value == '{' || value == '|' || value == '}' ||
           value == '~';
}

bool mailboxDomainValid(const std::string& domain) {
    if (domain.empty() || domain.size() > 255u) return false;
    if (domain.front() == '[' || domain.back() == ']') {
        if (domain.size() < 3u || domain.front() != '[' || domain.back() != ']')
            return false;
        bool hasSeparator = false;
        for (size_t index = 1u; index + 1u < domain.size(); ++index) {
            const unsigned char value = static_cast<unsigned char>(domain[index]);
            if (value == ':' || value == '.') {
                hasSeparator = true;
                continue;
            }
            if (!((value >= '0' && value <= '9') ||
                  (value >= 'a' && value <= 'f') ||
                  (value >= 'A' && value <= 'F')))
                return false;
        }
        return hasSeparator;
    }
    size_t labelStart = 0u;
    for (size_t index = 0u; index <= domain.size(); ++index) {
        if (index != domain.size() && domain[index] != '.') continue;
        const size_t labelSize = index - labelStart;
        if (labelSize == 0u || labelSize > 63u ||
            domain[labelStart] == '-' || domain[index - 1u] == '-')
            return false;
        for (size_t label = labelStart; label < index; ++label) {
            const unsigned char value = static_cast<unsigned char>(domain[label]);
            if (!((value >= 'A' && value <= 'Z') ||
                  (value >= 'a' && value <= 'z') ||
                  (value >= '0' && value <= '9') || value == '-'))
                return false;
        }
        labelStart = index + 1u;
    }
    return true;
}

bool mailboxAddressParse(const std::string& address, Mailbox& output,
                         const MailboxLimits& limits) {
    const size_t at = address.find('@');
    if (at == std::string::npos || at == 0u || at + 1u >= address.size() ||
        address.find('@', at + 1u) != std::string::npos ||
        address.size() > limits.maxAddressBytes)
        return false;
    const std::string local = address.substr(0u, at);
    const std::string domain = address.substr(at + 1u);
    if (local.size() > 64u || local.front() == '.' || local.back() == '.')
        return false;
    for (size_t index = 0u; index < local.size(); ++index) {
        const unsigned char value = static_cast<unsigned char>(local[index]);
        if (local[index] == '.' && index != 0u && local[index - 1u] == '.')
            return false;
        if (local[index] != '.' && !mailboxAtext(value)) return false;
    }
    if (!mailboxDomainValid(domain)) return false;
    output.localPart = local;
    output.domain = domain;
    output.address = address;
    return true;
}

bool mailboxDisplayParse(const std::string& source, std::string& output) {
    const std::string display = trim(source);
    if (display.empty()) {
        output.clear();
        return true;
    }
    if (display.front() == '"') {
        if (display.size() < 2u || display.back() != '"') return false;
        output.clear();
        for (size_t index = 1u; index + 1u < display.size(); ++index) {
            const unsigned char value = static_cast<unsigned char>(display[index]);
            if (value == '\\') {
                if (++index + 1u >= display.size()) return false;
                output.push_back(display[index]);
            } else {
                if (value < 0x20u || value == 0x7fu || value == '"') return false;
                output.push_back(display[index]);
            }
        }
        return true;
    }
    if (display.find('"') != std::string::npos ||
        display.find('<') != std::string::npos ||
        display.find('>') != std::string::npos ||
        display.find(',') != std::string::npos ||
        display.find(':') != std::string::npos)
        return false;
    for (unsigned char value : display)
        if (value < 0x20u || value == 0x7fu) return false;
    output = display;
    return true;
}

bool mailboxItemParse(const std::string& source, Mailbox& output,
                      const MailboxLimits& limits) {
    const std::string item = trim(source);
    if (item.empty()) return false;
    const size_t left = item.find('<');
    const size_t right = item.rfind('>');
    std::string address;
    if (left == std::string::npos || right == std::string::npos) {
        if (left != std::string::npos || right != std::string::npos ||
            item.find('"') != std::string::npos)
            return false;
        output.displayName.clear();
        address = item;
    } else {
        if (left == 0u || right <= left || right + 1u != item.size() ||
            item.find('<', left + 1u) != std::string::npos ||
            item.find('>', 0u) != right)
            return false;
        if (!mailboxDisplayParse(item.substr(0u, left), output.displayName))
            return false;
        address = trim(item.substr(left + 1u, right - left - 1u));
    }
    return output.displayName.size() <= limits.maxDisplayBytes &&
           mailboxAddressParse(address, output, limits);
}

bool mailboxListScan(const std::string& input, std::vector<Mailbox>& output,
                     const MailboxLimits& limits) {
    if (input.empty() || input.size() > limits.maxListBytes ||
        limits.maxEntries == 0u) return false;
    std::vector<Mailbox> candidate;
    size_t start = 0u;
    bool quoted = false;
    bool escaped = false;
    bool angle = false;
    for (size_t index = 0u; index <= input.size(); ++index) {
        const bool end = index == input.size();
        const char value = end ? ',' : input[index];
        if (!end) {
            if (quoted) {
                if (escaped) escaped = false;
                else if (value == '\\') escaped = true;
                else if (value == '"') quoted = false;
                continue;
            }
            if (value == '"') {
                quoted = true;
                continue;
            }
            if (value == '<') {
                if (angle) return false;
                angle = true;
                continue;
            }
            if (value == '>') {
                if (!angle) return false;
                angle = false;
                continue;
            }
        }
        if (value != ',' || quoted || angle) continue;
        if (candidate.size() >= limits.maxEntries ||
            !mailboxItemParse(input.substr(start, index - start),
                               candidate.emplace_back(), limits))
            return false;
        start = index + 1u;
    }
    if (quoted || escaped || angle || candidate.empty()) return false;
    output.swap(candidate);
    return true;
}

} // namespace

std::string Headers::get(const char* name) const {
    if (name == nullptr) return {};
    std::string wanted;
    size_t length = 0u;
    while (length < kMaxCStringHeaderNameBytes && name[length] != '\0')
        ++length;
    if (length == kMaxCStringHeaderNameBytes) return {};
    wanted.reserve(length);
    for (size_t index = 0u; index < length; ++index)
        wanted.push_back(asciiLower(name[index]));
    for (const Header& item : values)
        if (item.name == wanted) return item.value;
    return {};
}

bool parseHeaders(const std::string& input, Headers& result,
                  const Limits& limits) {
    Headers candidate;
    if (!parseHeaderBlock(input, candidate, limits)) return false;
    result = std::move(candidate);
    return true;
}

bool parseMailboxList(const std::string& input, std::vector<Mailbox>& result,
                      const MailboxLimits& limits) {
    std::vector<Mailbox> candidate;
    if (!mailboxListScan(input, candidate, limits)) return false;
    result.swap(candidate);
    return true;
}

bool parseParts(const std::string& raw, std::vector<Part>& parts, const Limits& limits) {
    parts.clear();
    if (raw.empty() || raw.size() > limits.maxRawBytes) return false;
    size_t separator = raw.find("\r\n\r\n");
    size_t skip = 4u;
    if (separator == std::string::npos) { separator = raw.find("\n\n"); skip = 2u; }
    if (separator == std::string::npos) return false;
    Headers root;
    if (!parseHeaderBlock(raw.substr(0u, separator), root, limits)) return false;
    return splitMime(root, raw.substr(separator + skip), {}, 0u, parts, limits);
}

} // namespace rinmime
